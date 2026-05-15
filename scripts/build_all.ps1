# Build all Makefile APP targets (Windows / cross-toolchain smoke check).
$ErrorActionPreference = "Stop"
$cc = "arm-none-eabi-gcc"
if (-not (Get-Command $cc -ErrorAction SilentlyContinue)) {
    $cc = "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gcc.exe"
}
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root
New-Item -ItemType Directory -Force -Path build | Out-Null

$apps = @(
    @{ n = "sched_demo"; m = "examples/sched_demo/main.c" },
    @{ n = "mutex_demo"; m = "examples/mutex_demo/main.c" },
    @{ n = "stress_test"; m = "examples/stress_test/main.c" },
    @{ n = "test_scheduler"; m = "tests/test_scheduler.c" },
    @{ n = "test_mutex"; m = "tests/test_mutex.c" },
    @{ n = "test_priority_inheritance"; m = "tests/test_priority_inheritance.c" },
    @{ n = "test_msgqueue"; m = "tests/test_msgqueue.c" },
    @{ n = "test_semaphore"; m = "tests/test_semaphore.c" },
    @{ n = "test_timeout"; m = "tests/test_timeout.c" }
)

$srcs = @(
    "platform/stm32f4/startup_stm32f407xx.s",
    "src/core/context_switch.s",
    "src/platform/system.c",
    "src/platform/syscalls.c",
    "src/memory/heap.c",
    "src/core/block.c",
    "src/core/scheduler.c",
    "src/core/task.c",
    "src/timer/sw_timer.c",
    "src/sync/mutex.c",
    "src/sync/semaphore.c",
    "src/ipc/msgqueue.c"
)

$flags = @(
    "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
    "-O0", "-g3",
    "-Iinclude", "-Isrc/core", "-Isrc/sync", "-Isrc/ipc", "-Isrc/memory", "-Isrc/timer",
    "-std=c11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-DQEMU_BUILD"
)

$ld = @(
    "-T", "platform/stm32f4/linker.ld",
    "-Wl,--gc-sections",
    "--specs=nano.specs", "--specs=nosys.specs", "-nostartfiles"
)

$fail = 0
foreach ($app in $apps) {
    $out = "build/$($app.n).elf"
    $args = $flags + $srcs + $app.m + $ld + @("-o", $out)
    & $cc @args
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL $($app.n)"
        $fail++
    } else {
        Write-Host "OK $($app.n)"
    }
}
if ($fail -gt 0) { exit 1 }
