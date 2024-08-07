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
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering;
use configparser::ini::Ini;
use std::thread;
use std::time;

static RUNNING: AtomicBool = AtomicBool::new(true);

#[derive(Debug, StructOpt)]
struct Opts {
	#[structopt(long, value_name = "CMD", help = "Top left corner command")]
	topleft: Option<String>,

	#[structopt(long, value_name = "CMD", help = "Top right corner command")]
	topright: Option<String>,

	#[structopt(long, value_name = "CMD", help = "Bottom right corner command")]
	bottomright: Option<String>,

	#[structopt(long, value_name = "CMD", help = "Bottom left corner command")]
	bottomleft: Option<String>,

	#[structopt(long, value_name = "CMD", help = "Left edge command")]
	left: Option<String>,

	#[structopt(long, value_name = "CMD", help = "Top edge command")]
	top: Option<String>,

	#[structopt(long, value_name = "CMD", help = "Right edge command")]
	right: Option<String>,

	#[structopt(long, value_name = "CMD", help = "Bottom edge command")]
	bottom: Option<String>,

	#[structopt(long, help = "Prints debug information")]
	debug: bool,

	#[structopt(long, short, help = "Read commands from config file")]
	config: bool,

	#[structopt(long, short, help = "Block until a command exits")]
	block: bool,

	#[structopt(long, short, value_name = "N", help = "Delay command execution for N milliseconds")]
	delay: Option<u64>,
}

#[derive(Debug)]
struct Commands {
	topleft: Option<String>,
	topright: Option<String>,
	bottomright: Option<String>,
	bottomleft: Option<String>,
	left: Option<String>,
	top: Option<String>,
	right: Option<String>,
	bottom: Option<String>,
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

fn sighandler() {
	RUNNING.store(false, Ordering::Relaxed);
}

fn point_in_rect(x: i32, y: i32, rect: (i32, i32, i32, i32)) -> bool
{
	let (rx, ry, rw, rh) = rect;

	if (rx <= x && x < rx + rw) &&
	   (ry <= y && y < ry + rh) {
		return true;
	}
	return false;
}

fn pointer_in_monitor(x: i32, y: i32, nmonitors: i32, monitorinfo: *const xrandr::XRRMonitorInfo) -> i32
{
	for i in 0..nmonitors {
		let rect = unsafe {
			((*monitorinfo.offset(i as isize)).x,
			 (*monitorinfo.offset(i as isize)).y,
			 (*monitorinfo.offset(i as isize)).width,
			 (*monitorinfo.offset(i as isize)).height)
		};

		if point_in_rect(x, y, rect) {
			return i;
		}
	}

	return -1;
}

fn get_xymax(x: i32, y: i32, xmax: &mut i32, ymax: &mut i32, display: *mut xlib::Display, nmonitors: i32, monitorinfo: *const xrandr::XRRMonitorInfo)
{
	unsafe {
		*xmax = xlib::XDisplayWidth(display, xlib::XDefaultScreen(display)) - 1;
		*ymax = xlib::XDisplayHeight(display, xlib::XDefaultScreen(display)) - 1;
	}

	if nmonitors == 1 {
		return;
	}

	let i = pointer_in_monitor(x, y, nmonitors, monitorinfo);
	if i < 0 {
		panic!("pointer_in_mointor failed");
	}

	unsafe {
		let w = (*monitorinfo.offset(i as isize)).width;
		let xoff = (*monitorinfo.offset(i as isize)).x;

		if xoff + w <= *xmax {
			*xmax = xoff + w - 1;
		}

		let h = (*monitorinfo.offset(i as isize)).height;
		let yoff = (*monitorinfo.offset(i as isize)).y;

		if yoff + h <= *ymax {
			*ymax = yoff + h - 1;
		}
	}
}

fn in_edge(x: i32, y: i32, xmax: i32, ymax: i32, offset: i32) -> Edge
{
	if x == 0 && y == 0 {
		return Edge::TOPLEFT;
	}
	if x == xmax && y == 0 {
		return Edge::TOPRIGHT;
	}
	if x == xmax && y == ymax {
		return Edge::BOTTOMRIGHT;
	}
	if x == 0 && y == ymax {
		return Edge::BOTTOMLEFT;
	}
	if x == 0 && y > offset && y < ymax - offset {
		return Edge::LEFT;
	}
	if y == 0 && x > offset && x < xmax - offset {
		return Edge::TOP;
	}
	if x == xmax && y > offset && y < ymax - offset {
		return Edge::RIGHT;
	}
	if y == ymax && x > offset && x < xmax - offset {
		return Edge::BOTTOM;
	}
	return Edge::NONE;
}

fn run(opts: &Opts, edge: Edge, cmds: &Commands)
{
	let cmd = match edge {
		Edge::TOPLEFT => &cmds.topleft,
		Edge::TOPRIGHT => &cmds.topright,
		Edge::BOTTOMRIGHT => &cmds.bottomright,
		Edge::BOTTOMLEFT => &cmds.bottomleft,
		Edge::LEFT => &cmds.left,
		Edge::TOP => &cmds.top,
		Edge::RIGHT => &cmds.right,
		Edge::BOTTOM => &cmds.bottom,
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
	if split.is_empty() {
		return;
	}
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

fn query_pointer(display: *mut xlib::Display, window: xlib::Window) -> (i32, i32)
{
	let mut root_ret: u64 = 0;
	let mut child_ret: u64 = 0;
	let mut x: i32 = 0;
	let mut y: i32 = 0;
	let mut winx_ret: i32 = 0;
	let mut winy_ret: i32 = 0;
	let mut mask_ret: u32 = 0;

	unsafe {
		xlib::XQueryPointer(display,
				    window,
				    &mut root_ret,
				    &mut child_ret,
				    &mut x,
				    &mut y,
				    &mut winx_ret,
				    &mut winy_ret,
				    &mut mask_ret);
	}

	return (x, y);
}

fn main()
{
	let opts = Opts::from_args();

	// Set delay
	let default_delay: u64 = 0;
	let max_delay: u64 = 1000;
	let delay: u64 = opts.delay.unwrap_or(default_delay).min(max_delay);

	// Set commands from arguments
	let mut cmds = Commands {
		topleft: opts.topleft.clone(),
		topright: opts.topright.clone(),
		bottomright: opts.bottomright.clone(),
		bottomleft: opts.bottomleft.clone(),
		left: opts.left.clone(),
		top: opts.top.clone(),
		right: opts.right.clone(),
		bottom: opts.bottom.clone(),
	};

	// Load commands from file
	if opts.config {
		let mut cfg = Ini::new();
		let mut path = dirs::config_dir().unwrap();
		path.push("edges.conf");
		if let Err(err) = cfg.load(path) {
			panic!("{}", err);
		}
		cmds.topleft = cfg.get("commands", "topleft");
		cmds.topright = cfg.get("commands", "topright");
		cmds.bottomright = cfg.get("commands", "bottomright");
		cmds.bottomleft = cfg.get("commands", "bottomleft");
		cmds.left = cfg.get("commands", "left");
		cmds.top = cfg.get("commands", "top");
		cmds.right = cfg.get("commands", "right");
		cmds.bottom = cfg.get("commands", "bottom");
	}

	// Check if we run on Wayland
	if let Ok(_) = env::var("WAYLAND_DISPLAY") {
		panic!("Global pointer query not supported on Wayland");
	}

	unsafe {
		// Catch signals
		libc::signal(libc::SIGINT, sighandler as usize);
		libc::signal(libc::SIGTERM, sighandler as usize);
		libc::signal(libc::SIGHUP, sighandler as usize);

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

		// Get monitors
		let mut nmonitors: i32 = 0;
		let monitorinfo = xrandr::XRRGetMonitors(display, window, xlib::True, &mut nmonitors);
		if monitorinfo.is_null() {
			panic!("XRRGetMonitors failed");
		}

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

		while RUNNING.load(Ordering::Relaxed) {
			if xlib::XPending(display) == 0 {
				continue;
			}

			let event = {
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

				let (x, y) = query_pointer(display, window);

				if opts.debug {
					println!("{} {}", x, y);
				}

				get_xymax(x, y, &mut xmax, &mut ymax, display, nmonitors, monitorinfo);

				// Specifies the "hot" zones
				let offset: i32 = ((ymax as f64) * 0.25) as i32;

				// Make sure we run commands only once on edge hits
				if (x == oldx && y == oldy) ||
				   (x == oldx && y > offset && y < ymax - offset) ||
				   (y == oldy && x > offset && x < xmax - offset) {
					xlib::XFreeEventData(display, &mut cookie);
					continue;
				}

				let edge = in_edge(x, y, xmax, ymax, offset);

				if edge != Edge::NONE {
					if opts.debug {
						println!("delay: {}", delay);
					}

					// Apply delay
					thread::sleep(time::Duration::from_millis(delay));

					// Run the command if the pointer is still in the edge
					let (x, y) = query_pointer(display, window);
					if edge == in_edge(x, y, xmax, ymax, offset) {
						run(&opts, edge, &cmds);
					}
				}

				oldx = x;
				oldy = y;
			}

			xlib::XFreeEventData(display, &mut cookie);

			// Wait for events
			if libc::poll(&mut fds, 1, -1) < 0 {
				panic!("poll failed");
			}
		};

		// Clean up
		xrandr::XRRFreeMonitors(monitorinfo);
		xlib::XCloseDisplay(display);
	}
}
