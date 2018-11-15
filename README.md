# Ceton Drivers

## Versioning

To update the version numbers run the below

__ctn_ver.cmd

Note: Do not run between x86-64 and x86-32 builds, we want the versions to match.

## Build

Open x86 and AMD64 based W7 DDK command prompt and run the below

__build.cmd

To include the driver debug symbols in our symbol server run

__build.cmd /release

## Signing

From x86-64 command prompt run __sign.cmd