# zypak

Allows you to run Chromium based applications that require a sandbox in a Flatpak environment,
by using LD_PRELOAD magic and a redirection system that redirects Chromium's sandbox to use
the Flatpak sandbox.

## Basic usage

This requires your Flatpak to be using:

- `org.freedesktop.Platform` / `Sdk` version `19.08` or later.
- `org.electronjs.Electron2.BaseApp` as your base app. Recent releases include Zypak
  built-in.

Now, instead of running your Electron binary directly, call it via
`zypak-wrapper PATH/TO/MY/ELECTRON/BINARY`.

## Re-exec behavior

By default, Zypak will detect when the app is re-exec'ing itself and attempt to
spawn the latest version, in order for `chrome://restart` to function. This can
be disabled by setting `ZYPAK_SPAWN_LATEST_ON_REEXEC=0`. **This will be the
default in the future once existing browsers are migrated,** because it matches
more closely with what Electron would expect.

### Usage with a wrapper script

If `ZYPAK_SPAWN_LATEST_ON_REEXEC=1` (the current default) is active, and Zypak
was invoked by some sort of wrapper script, make sure you set `CHROME_WRAPPER=`
to the path of said script. Otherwise, if the application attempts to re-exec
itself (i.e. `chrome://restart`), it won't be using the wrapper on re-exec,
leading to potentially unexpected behavior.

## Widevine support

Chromium and variants often cannot legally distribute Widevine themselves, so the binaries are
downloaded at runtime, usually into a folder named `WidevineCdm` located somewhere under the
browser's data storage directory. For instance:

- Chromium downloads Widevine to:
  `~/.var/app/org.chromium.Chromium/config/chromium/WidevineCdm`
- Brave downloads Widevine to:
  `~/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser/WidevineCdm`

This directory will also usually contain one or more of these files:

- `latest-component-updated-widevine-cdm`
- `manifest.json`

In order for the Widevine path to be exposed to the sandbox, you must set
`ZYPAK_EXPOSE_WIDEVINE_PATH=` to the full path to this Widevine directory. Otherwise, the CDM module
will be downloaded, but the browser will be unable to load it.

The easiest way to test if Widevine is working is [this test page](https://bitmovin.com/demos/drm);
if `ZYPAK_EXPOSE_WIDEVINE_PATH=` was set incorrectly, you'll see a message like this:

```
Unable to instantiate a key system supporting the required combinations
(DRM_NO_KEY_SYSTEM)
```

## Alternate sandbox binary names

Some applications like Microsoft Edge use a custom file name for the sandbox binary name, rather
than the default of `chrome-sandbox`. In that case, you may see messages like this:

```
The SUID sandbox helper binary was found, but is not configured correctly. Rather than run without
sandboxing I'm aborting now. You need to make sure that /app/extra/msedge-sandbox is owned by root
and has mode 4755.
```

To fix this, set `ZYPAK_SANDBOX_FILENAME=the-sandbox-basename`, e.g.
`ZYPAK_SANDBOX_FILENAME=msedge-sandbox`.

## Setting `LD_PRELOAD`

Zypak needs to override `LD_PRELOAD` in order to inject its redirection libraries into the
application process. If you need to add your own libraries to `LD_PRELOAD`, place them in
`ZYPAK_LD_PRELOAD`, which will result in Zypak adding them to the `LD_PRELOAD` list, in addition to
its own required libraries.

## Using a different version

If you want to try a different Zypak version for testing, or without using the
Electron baseapp, then find the release tag you want to use and add one of these
modules somewhere in your Flatpak manifest:

```json
{
  "name": "zypak",
  "sources": [
    {
      "type": "git",
      "url": "https://github.com/refi64/zypak",
      "tag": "THE_RELEASE_TAG"
    }
  ]
}
```

```yaml
- name: zypak
  sources:
    - type: git
      url: https://github.com/refi64/zypak
      tag: THE_RELEASE_TAG
```

## Debugging

- Set `ZYPAK_DEBUG=1` to enable debug logging.
- Set `ZYPAK_STRACE=all` to run strace on the host and child processes.
  - To make it host-only or child-only, set `ZYPAK_STRACE=host` or `ZYPAK_STRACE=child`, respectively.
  - If only some child processes should be searched, use `ZYPAK_STRACE=child:type1,type2,...`, e.g.
    `ZYPAK_STRACE=child:ppapi,utility` to trace all children of `--type=utility` and `--type=ppapi`.
  - Set `ZYPAK_STRACE_FILTER=expr` to pass a filter expression to `strace -e`.
  - In order to avoid arguments being ellipsized, set `ZYPAK_STRACE_NO_LINE_LIMIT=1`.
- Set `ZYPAK_DISABLE_SANDBOX=1` to disable the use of the `--sandbox` argument
  (required if the Electron binary is not installed, as the sandboxed calls will be unable to locate the Electron binary).

## How does it work?

Zypak works by using LD_PRELOAD to trick Chromium into thinking its SUID sandbox is present and still
setuid, but all calls to it get instead redirected to another binary: Zypak's own sandbox.

This sandbox has two strategies to sandbox Chromium:

### The mimic strategy

The *mimic strategy* works on the majority of Flatpak versions. It works by mimicking the zygote
and redirecting all the incoming commands to actually become `flatpak-spawn` commands, then
returning those PIDs as the results of the "fork". This *does* have the side effect of slower
startup and higher memory usage, since there is no true zygote running, and thus this is only used
where the spawn strategy (see below) does not work.

### The spawn strategy

The *spawn strategy* a far better implementation, available on all Flatpak versions 1.8.2+. (Flatpak
1.8.0 and 1.8.1 are not really supported.) It relies on two particular new features in 1.8.0:
`expose-pids` and `SpawnStarted`:

- `expose-pids` lets the process that opens a new sandbox see the PIDs of the sandboxed processes.
  This essentially means it behaves much like using user namespaces to perform sandboxing and allows
  Chromium to see the true PIDs of its child processes rather than trying to use an intermediary
  (`flatpak-spawn` in the mimic strategy).
- `SpawnStarted` is emitted when a sandboxed process fully starts, and it passes along the PID that
  can be used for the parent to reach the sandboxed children.

In this strategy, the zygote is no longer mimicked; rather, the actual zygote is run sandboxed, just
like Chromium's official sandboxes work. The only difference is, the Flatpak sandbox is used instead
of Chromium's setuid or namespace sandboxes.

This is a bit messy because Flatpak's sandboxing APIs all use D-Bus, so a new D-Bus session must be
"injected" into the main Chromium process, which then runs in a separate thread and handles all the
sandbox functionality. When the separate zypak-sandbox binary is started, it talks to this
"supervisor" thread via a local socket pair, asking it to run the sandboxed process and staying
alive until the sandboxed process completes. Meanwhile, the supervisor thread will swap out the
sandbox PID for the true sandboxed process PID.

### Rough layout of execution

- When you use `zypak-wrapper`, it sets up the paths to the Zypak binary and
  library directories and then calls `zypak-helper`.
- `zypak-helper` will set up the LD_PRELOAD environment and start the main
  process.
    - If the spawn strategy is being used, a supervisor thread is started to manage the
      sandboxed processes and communication with Flatpak.
- When Chromium attempts to launch a sandboxed process, `zypak-sandbox` is used as the
  sandbox instead of the SUID sandbox, and it then does one of the following:
  - If the mimic strategy is being used, Zypak's mimic zygote will run, replacing
    the true zygote. All the zygote messages get handled, and process forks instead
    start a new process via `flatpak-spawn`.
  - If the spawn strategy is being used, the sandbox will send a message to the supervisor
    to start a new sandboxed process, then wait for the sandboxed process to exit.
