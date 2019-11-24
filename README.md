# zypak

Allows you to run Electron binaries that require a sandbox in a Flatpak environment, by using
LD_PRELOAD magic and a redirected sandbox.

## Basic usage

This requires your Flatpak to be running an SDK equal to or newer than Freedesktop SDK 19.08.

Find the latest tagged release, then add one of these modules somewhere in your Flatpak manifest:

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

Then, instead of running your Electron binary directory, call it via
`zypak-wrapper PATH/TO/MY/ELECTRON/BINARY`.

## Debugging

- Set `ZYPAK_DEBUG=1` or pass `-d` to `zypak-wrapper` to enable debug logging.
- Set `ZYPAK_DISABLE_SANDBOX=1` to disable the use of the `--sandbox` argument (
  required if the Electron binary is not installed, as the sandboxed calls will be unable to
  locate the Electron binary).

## Random notes

This has been successfully lightly tested with Chromium itself.
