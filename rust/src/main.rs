use structopt::StructOpt;
use x11::xlib;
use x11::xrandr;
use x11::xinput2;
use std::ptr;
use std::env;
use std::ffi::CString;
use std::mem::MaybeUninit;
use std::ffi::CStr;
use nix::unistd;
use nix::sys;

#[derive(Debug, StructOpt)]
struct Opts {
	#[structopt(long, help = "Top left corner command")]
	topleft: Option<String>,

	#[structopt(long, help = "Top right corner command")]
	topright: Option<String>,

	#[structopt(long, help = "Bottom right corner command")]
	bottomright: Option<String>,

	#[structopt(long, help = "Bottom left corner command")]
	bottomleft: Option<String>,

	#[structopt(long, help = "Left edge command")]
	left: Option<String>,

	#[structopt(long, help = "Top edge command")]
	top: Option<String>,

	#[structopt(long, help = "Right edge command")]
	right: Option<String>,

	#[structopt(long, help = "Bottom edge command")]
	bottom: Option<String>,

	#[structopt(long, short, help = "Prints debug information")]
	debug: bool,

	#[structopt(long, short, help = "Read commands from config file")]
	config: bool,

	#[structopt(long, short, help = "Block until a command exits")]
	block: bool,
}

#[derive(Debug, PartialEq)]
enum Edge {
	TOPLEFT,
	TOPRIGHT,
	BOTTOMRIGHT,
	BOTTOMLEFT,
	LEFT,
	TOP,
	RIGHT,
	BOTTOM,
	NONE,
}

fn get_xymax(xmax: &mut i32, ymax: &mut i32, display: *mut xlib::Display)
{
	unsafe {
		*xmax = xlib::XDisplayWidth(display, xlib::XDefaultScreen(display)) - 1;
		*ymax = xlib::XDisplayHeight(display, xlib::XDefaultScreen(display)) - 1;
	}
}

fn in_edge(x: i32, y: i32, xmax: i32, ymax: i32, offset: i32) -> Edge
{
	if x == 0 && y == 0 {
		return Edge::TOPLEFT;
	} else if x == xmax && y == 0 {
		return Edge::TOPRIGHT;
	} else if x == xmax && y == ymax {
		return Edge::BOTTOMRIGHT;
	} else if x == 0 && y == ymax {
		return Edge::BOTTOMLEFT;
	} else if x == 0 && y > offset && y < ymax - offset {
		return Edge::LEFT;
	} else if y == 0 && x > offset && x < xmax - offset {
		return Edge::TOP;
	} else if x == xmax && y > offset && y < ymax - offset {
		return Edge::RIGHT;
	} else if y == ymax && x > offset && x < xmax - offset {
		return Edge::BOTTOM;
	} else {
		return Edge::NONE;
	}
}

fn run(opts: &Opts, edge: Edge)
{
	let cmd = match edge {
		Edge::TOPLEFT => &opts.topleft,
		Edge::TOPRIGHT => &opts.topright,
		Edge::BOTTOMRIGHT => &opts.bottomright,
		Edge::BOTTOMLEFT => &opts.bottomleft,
		Edge::LEFT => &opts.left,
		Edge::TOP => &opts.top,
		Edge::RIGHT => &opts.right,
		Edge::BOTTOM => &opts.bottom,
		_ => &None,
	};

	if opts.debug {
		println!("{:?}: {:?}", edge, cmd);
	}

	if cmd.is_none() {
		return;
	}

	// Split to C strings for execvp
	let s: String = cmd.as_ref().unwrap().to_string();
	let split: Vec<&str> = s.split_whitespace().collect();
	let vec: Vec<CString> = split.iter().map(|s| CString::new(s.as_bytes()).unwrap()).collect();
	let args: Vec<&CStr> = vec.iter().map(|c| c.as_c_str()).collect();

	unsafe {
		match unistd::fork() {
			Ok(unistd::ForkResult::Child) => {
				let _ = unistd::execvp(args[0], &args);
				libc::_exit(0);
			}
			Ok(unistd::ForkResult::Parent {..}) => {
				if opts.block {
					let _ = sys::wait::wait();
				}
			}
			Err(_) => println!("Fork failed"),
		}
	}
}

fn main()
{
	let opts = Opts::from_args();
	//println!("{:?}", opts);

	// Check if we run on Wayland
	if let Ok(_) = env::var("WAYLAND_DISPLAY") {
		panic!("Global pointer query not supported on Wayland");
	}

	unsafe {
		// Open display
		let display = xlib::XOpenDisplay(ptr::null());
		if display.is_null() {
			panic!("XOpenDisplay failed");
		}

		let window = xlib::XDefaultRootWindow(display);

		// Query XInput2
		let mut major_opcode: i32 = 0;
		let mut first_event: i32 = 0;
		let mut first_error: i32 = 0;
		let c_str = CString::new("XInputExtension").unwrap();

		if xlib::XQueryExtension(display,
					 c_str.as_ptr(),
					 &mut major_opcode,
					 &mut first_event,
					 &mut first_error) == xlib::False {
			panic!("Failed to query XInputExtension");
		}

		// Query Xrandr
		let mut have_randr_1_5: bool = false;
		let mut event_base: i32 = 0;
		let mut error_base: i32 = 0;

		if xrandr::XRRQueryExtension(display, &mut event_base, &mut error_base) == xlib::True {
			let mut major: i32 = 0;
			let mut minor: i32 = 0;

			xrandr::XRRQueryVersion(display, &mut major, &mut minor);

			if (major == 1 && minor >= 5) || major > 1 {
				have_randr_1_5 = true;
			}
		}

		if !have_randr_1_5 {
			panic!("Xrandr >= 1.5 not available");
		}

		// Select raw motion events
		let mut mask = [0u8; (xinput2::XI_LASTEVENT as usize + 7) / 8]; // wtf?
		xinput2::XISetMask(&mut mask, xinput2::XI_RawMotion);

		let mut event_mask = xinput2::XIEventMask {
			deviceid: xinput2::XIAllMasterDevices,
			mask_len: mask.len() as i32,
			mask: &mut mask[0] as *mut u8,
		};
		xinput2::XISelectEvents(display, window, &mut event_mask, 1);

		// prepare polling
		let mut fds = libc::pollfd {
			fd: xlib::XConnectionNumber(display),
			events: libc::POLLIN,
			revents: 0,
		};

		// Main loop

		let mut oldx: i32 = 1;
		let mut oldy: i32 = 1;
		let mut xmax: i32 = 0;
		let mut ymax: i32 = 0;

		loop {
			if xlib::XPending(display) == 0 {
				continue;
			}

			let mut event = {
				let mut event = MaybeUninit::uninit();
				xlib::XNextEvent(display, event.as_mut_ptr());
				event.assume_init()
			};

			let mut cookie: xlib::XGenericEventCookie = event.generic_event_cookie;
			xlib::XGetEventData(display, &mut cookie);

			// Was pointer moved?
			if cookie.type_ == xlib::GenericEvent &&
			   cookie.extension == major_opcode &&
			   cookie.evtype == xinput2::XI_RawMotion {

				let mut root_ret: u64 = 0;
				let mut child_ret: u64 = 0;
				let mut x: i32 = 0;
				let mut y: i32 = 0;
				let mut winx_ret: i32 = 0;
				let mut winy_ret: i32 = 0;
				let mut mask_ret: u32 = 0;

				xlib::XQueryPointer(display,
						    window,
						    &mut root_ret,
						    &mut child_ret,
						    &mut x,
						    &mut y,
						    &mut winx_ret,
						    &mut winy_ret,
						    &mut mask_ret);

				// Now we have the position in x and y

				if opts.debug {
					println!("{} {}", x, y);
				}

				get_xymax(&mut xmax, &mut ymax, display);

				// Specifies the "hot" zones
				let offset: i32 = ((ymax as f64) * 0.25) as i32;

				// Make sure we run commands only once on edge hits
				if (x == oldx && y == oldy) ||
				   (x == oldx && y > offset && y < ymax - offset) ||
				   (y == oldy && x > offset && x < xmax - offset) {
					xlib::XFreeEventData(display, &mut event.generic_event_cookie);
					continue;
				}

				let edge = in_edge(x, y, xmax, ymax, offset);
				if edge != Edge::NONE {
					run(&opts, edge);
				}

				oldx = x;
				oldy = y;
			}

			xlib::XFreeEventData(display, &mut event.generic_event_cookie);

			if libc::poll(&mut fds, 1, -1) < 0 {
				break;
			}
		};

		// Close display
		xlib::XCloseDisplay(display);
	}
}
