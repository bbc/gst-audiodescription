[Gstreamer](https://gstreamer.freedesktop.org/) elements implementing some
suport for rendering [Audio Description](https://en.wikipedia.org/wiki/Audio_description).

The elements are,
 * *whp198dec* - extracts ``AD_descriptor`` structures from an audio waveform, encoded per [BBC R&D whitepaper WHP 198](http://www.bbc.co.uk/rd/publications/whitepaper198)
 * *adcontrol* - consumes buffers of ``AD_descriptor`` structures and uses these to control an internal 'volume' element; used to implement the 'fading' of the audio of the main presentation as required for the audio description content to be heard clearly


                   +-------------+
    WHP 198 signal |             |
    +-------------->  whp198dec  |
                   |             |
                   +-------------+
                          |
                          | AD_descriptors
                          |
                   +------v------+
    Main audio     |             |  Main audio (with fade)
    +-------------->  adcontrol  +--------------->
                   |             |
                   +-------------+
                                    Description audio
    +-------------------------------------------->


## Limitations


 * Volume changes for the main track are auidably abrupt - there's no interpolation between 'fade' control steps
 * Volume changes are not explicitly synchronised to the audio stream, which might cause problems for some pipeline structures (untested)
 * The _whp198dec_ element hs not been generalized to support multiple sample formats and bit rates - use other Gestreamer elements to convert as required
 * Ignores 'pan' information (I have no example content using the panning feature)


## Example pipeline

Given a ``test.wav`` contains description in the left stereo channel, and the _WHP 198_ control data in the right channel, this pipeline plays the description track using a noise test signal for the main audio, as a basic demo of the control over the main audio's volume level. 

    gst-launch-1.0 \
	  filesrc location=test.wav \
		! wavparse \
		! deinterleave name=d \
	  d.src_1 \
		! queue max-size-time=100000000 \
		! audioconvert \
		! audio/x-raw,format=S16LE,rate=48000,channels=1 \
		! whp198dec \
		! ad. \
	  d.src_0 \
		! queue max-size-time=100000000 \
		! audioconvert \
		! audio/x-raw,format=S16LE,rate=48000,channels=1 \
		! mix. \
	  audiotestsrc wave=red-noise volume=0.3 \
		! audio/x-raw,format=S16LE,rate=48000,channels=1 \
		! adcontrol name=ad \
		! mix. \
	  audiomixer name=mix \
		! autoaudiosink


