# Installing

Each release on the repository's
[Releases page](https://github.com/simoninns/speak5220/releases) has an
installer for Linux, Windows and macOS, and a `.sha256` file with each
installer's checksum. Each installs two programs: `speak5220`, the
application described here, and `speak5220-cli`, the
[command-line tool](user-manual/cli.md). To build them from source
instead, see `BUILD.md` in the repository.

The installers are not signed with a paid certificate, so Windows and
macOS warn about them the first time. The steps below get past the
warning.

## Linux (Flatpak)

Install the `.flatpak` file, then start Speak5220 from the desktop's menu
or with `flatpak run`:

```bash
flatpak install --user Speak5220-1.0.0-linux-x86_64.flatpak
flatpak run io.github.simoninns.speak5220
```

Flatpak fetches the KDE runtime it needs from Flathub. The application can
open and save files anywhere in your home directory, but not outside it.

## Windows (MSI)

1. Open the `.msi` file. Windows SmartScreen may say **Windows protected
   your PC**: choose **More info**, then **Run anyway**.
2. Follow the installer. It puts Speak5220 in
   `C:\Program Files\Speak5220\` and adds it to the Start menu. Windows
   may name the publisher as *Unknown*.

To remove it, use **Settings > Apps > Installed apps**. A newer MSI
replaces an older one.

## macOS (DMG)

The DMG runs on macOS 13 or later, on Apple silicon and Intel Macs.

1. Open the `.dmg` file and drag **speak5220** to **Applications**.
2. Open it from Applications. macOS says it cannot check the application
   for malicious software (*"speak5220" Not Opened*): choose **Done**.
3. Open **System Settings > Privacy & Security**, scroll to
   **Security**, and choose **Open Anyway** beside the message about
   speak5220. Confirm with your password, then choose **Open**.

macOS asks only the first time. Alternatively, from Terminal:

```bash
xattr -dr com.apple.quarantine /Applications/speak5220.app
```

Then see [making speech](making-speech.md) for a walkthrough.
