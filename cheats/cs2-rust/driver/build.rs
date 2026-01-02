//! Build script - no WDK required
fn main() {
    // Link to ntoskrnl for kernel functions
    println!("cargo:rustc-link-lib=ntoskrnl");
}
