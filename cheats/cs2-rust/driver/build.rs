//! Build script for kernel driver
//! Configures WDK (Windows Driver Kit) environment

fn main() -> Result<(), wdk_build::ConfigError> {
    wdk_build::configure_wdk_binary_build()
}

