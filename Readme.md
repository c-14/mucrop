# mucrop

A minimal interactive image cropper

A simple interactive image cropping utility using ImageMagick and libxcb.

Current Status:

	Release - 0.1.0
		This is the initial release. All necessary features are included, but may not be feature-complete.

## INSTALL

see [INSTALL](INSTALL)

## USAGE

    mucrop <src_filename> [dst_filename]

### KEYBINDINGS

*   w: writes the cropped image to <dst_filename> if given, otherwise rewrites <src_filename>
*   q: quits without writing
* ESC: cancels the current crop operation
