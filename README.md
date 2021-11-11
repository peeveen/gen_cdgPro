# gen_cdgPro
CDG display plugin for Winamp.

![Lyrics](/media/lyrics.gif?raw=true)
![Logo](/media/logo.gif?raw=true)

* Configurable transparency, to allow visualization (e.g. [Milkdrop](http://www.mywinamp.com/milkdrop/)) to show through.
* Option to add non-transparent outline around foreground graphics to improve readability.
* [Scale2X smoothing](https://www.scale2x.it/) to de-blockify the graphics. Multiple passes can be configured.
* Display your own logo when no CDG file is playing.
* Works with zipped or unzipped MP3+CDG files.
* Fast!

### Installation
* Download and extract the ZIP from the [Releases](https://github.com/peeveen/gen_cdgPro/releases) page.
* Edit the INI file with your own preferences. Pay attention to the comments in that file.
* Copy the two files (DLL & INI) to your Winamp plugins folder (usually "C:\Program Files (x86)\Winamp\Plugins")
* Restart Winamp.
* Next time you play a MP3+CDG file (zipped or otherwise), the CDGPro window should appear.

### Known problems
If you are using Winamp 5.8, it can be difficult to drag the CDG window, as you cannot "grab" the transparent areas.
This doesn't seem to be a problem in Winamp 5.666.

### TODO

* Prefs UI dialog (currently just an INI file).
* Improve error handling.
* More comments.

If you like this, and/or use it commercially, please consider throwing some coins my way via PayPal, at steven.fullhouse@gmail.com

Thanks!
