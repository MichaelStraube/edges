fn main() {
    println!("cargo:rustc-link-lib=X11");
    println!("cargo:rustc-link-lib=Xrandr");
    println!("cargo:rustc-link-lib=Xi");
}

