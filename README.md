# Replay

Watch back your best scores!

## FAQ (Frequently Asked Questions)

- How do I record a replay?
  - For now, the only way to record replays is with [BeatLeader](https://github.com/BeatLeader/beatleader-qmod/releases). All you have to do is play any level with BeatLeader installed. Replay will save all replays from BeatLeader, and add a replay button next to the play button for levels that have a replay.
  - Replays from the old 1.17.1 version of this mod are available to view, but cannot be recorded. Recording them will never be added because their format is much worse for viewing and rendering.

- What is the purpose of the Render Video feature?
  - The Render Video feature allows Quest gameplay to look identical to PC recordings. You can now set FOV, mirrors, shockwaves, etc all running smoothly at the framerate you want.
  - It will output both video and audio files which need to be mixed together afterwards. See [processing renders](#processing-renders).

- Why is my Quest randomly crashing while rendering?
  - If your Quest isn’t normally crashing either during normal replays or out of replays entirely, this is most likely due to the Quest running out of memory. This can especially be the case if you are playing an NE or Chroma map, or rendering at 4k. Try lowering your video resolution and bitrate. This is not fixable.
  - Alternatively, try rendering one or two more times and/or restarting your Quest. There is a (relatively small) chance it won't crash.

- The gameplay in my headset looks messed up while using the rendering feature (missing hud, trails, etc), what can I do to fix this?
  - This is caused by the Quest being under a lot of stress while rendering. This will not be seen on the finished video, so it can be safely ignored.
  - Shockwaves and distortions will always be broken on the headset camera.

- My Quest has gone to sleep while during a render. What effects will this have?
  - If you have Allow Pauses disabled, the render will continue the second the Quest has woken up and the finished video will have no effects.
  - If you have it enabled, the render will have the pause menu in it. If you unpause, it will still continue rendering the level, but you may want to edit it out.

- After mixing the audio with the video, it doesn’t sync up perfectly, how can I fix this?
  - For now this is how it will always be. After mixing the audio, you can fix the audio in the average free video editor.
  - If you extract the song from the map file, then it will almost always be synchronized without needing to do anything, at the cost of hit sounds and any other sound effects.

## Rendering tips

- Rendering will pause if the headset goes to sleep. If you have adb, you can disable the proximity sensor and prevent it from falling asleep with the command `adb shell am broadcast -a com.oculus.vrpowermanager.prox_close`. It will be enabled again if the headset restarts, or if you run the command `adb shell am broadcast -a com.oculus.vrpowermanager.automation_disable`.

- Rendering at a resolution higher than 1080p may cause Beat Saber to run out of memory and crash.

- Closing the game during a render will most likely cause the output to be corrupt. If you want to stop it before the level is finished, enable "Allow Pauses" in the rendering section of the mod settings before starting the render and exit through the pause menu.

## Processing renders

__This guide assumes that you know how to use adb, Window File Explorer or SideQuest Advanced Installer (if you aren’t on Windows) to access the Quest’s files.__

After rendering a video using the mod, two files will be created in the `Renders` folder in the top level directory of your quest, a `.h264` and a `.wav`, which are the video and audio outputs respectively. The fastest way to combine these is to copy the video codec, see below on how to do this with FFMPEG (HandBrake does not support this).

### Using HandBrake

HandBrake is a user-friendly option for processing renders if you want to avoid the command line. However, it cannot add audio to the video - you will have to use either any other video editor or ffmpeg (see below) to do that.

- Install and open [HandBrake](https://handbrake.fr/).
- Drag the `.h264` file into HandBrake. If successfully detected, it will then show you a preview of the video.
- Select the `Dimensions` tab. 
  - Under `Orientation & Cropping`, set `Cropping` to `None`.
  - Under `Resolution & Scaling`, set `Resolution Limit` to whatever resolution you have rendered the video at (if you are unsure, check the Replay mod settings).
- Select the `Video` tab.
   - Set the `Framerate (FPS)` setting to whatever framerate you rendered at (again, check the mod settings if you are unsure).
   - Below the framerate setting, select `Constant Framerate`.
   - In the `Quality` section, select `Average Bitrate (kbps)` and in the text box next to it put the bitrate you rendered at (which can be found in the Replay mod settings).
   - Optionally, deselect `Turbo First Pass` for a higher quality, but be warned: this will drastically increase conversion time.
- You can now hit the `Start` button to begin conversion. This will take a while. You can view the progress in the bottom left.

You can prevent having to set these settings every time you launch HandBrake by selecting the `Presets` button on the top right, and selecting the `+` in the bottom left of the new window that has opened. You will now be able to select that preset every time you open HandBrake.

### Using FFMPEG

[FFMPEG](https://ffmpeg.org/) is a command line tool with a lot of power, but all we need it for here is to convert the video and mix in some audio, which only takes one command and then all the work will be done (assuming the video and audio are synchronized). What command to run depends on your desired outcome. If you want a large file size with the best quality, click No Compression. If you want a smaller file size with worse quality, click Compression

<details>
<summary>No Compression</summary>
<br>

Even without compression, you have some control over the file size. To get the largest file size, but the fastest encoding, use -c:v copy:

`ffmpeg -i "path/to/video.h264" -i "path/to/audio.wav" -c:v copy "path/to/output/video.mp4"`

Reencoding may result in a smaller file size but will take some time. To reencode, just exclude -c:v copy:

`ffmpeg -i "path/to/video.h264" -i "path/to/audio.wav" "path/to/output/video.mp4"`

If you have a GPU with support for hardware encoding, you can reencode using it, which may result in a smaller file size and is faster than reencoding without it. First, find the manufacturer of your gpu

<details>
<summary>Finding GPU manufacturer on windows</summary>
<br>

- Open task manager with the keybind `Ctrl + Shift + Escape` or `Ctrl + Alt + Delete` and selecting the option for it. 
- Open the `Performance` tab. This should show statistics about your PC such as the CPU, Memory, and all attached disks. Select the first option labelled GPU.
- Across on the top, it should say the model of your GPU. Look for either `AMD`, `NVIDIA`, or `Intel`.
</details>

Next, find the codec for your gpu manufacturer
| Manufacturer | Codec |
| --- | --- |
| Intel | `h264_qsv` |
| NVIDIA | `h264_nvenc` |
| AMD | `h264_amf` |

Then, use that codec:
`ffmpeg -i "path/to/video.h264" -i "path/to/audio.wav" -c:v replace_with_codec "path/to/output/video.mp4"`
</details>
<details>
<summary>Compression</summary>
<br>

To compress the output video, use this command:

`ffmpeg -i "path/to/video.h264" -i "path/to/audio.wav" -c:v libx264 -crf amount "path/to/output/video.mp4"`

Replace "amount" with a value between 0-51. The larger the amount the smaller the file size will be, but quality will be worse. A value of 19 is perceptually lossless, resulting in a smaller file size with about the same quality.

Some hardware codecs also support compression, which can make compressing a lot faster. For Nvidia GPUs, use this command:

`ffmpeg -i "path/to/video.h264" -i "path/to/audio.wav" -c:v h264_nvenc -preset:v p7 -tune:v hq -rc:v vbr -cq:v amount -b:v 0 -profile:v high "path/to/output/video.mp4"`

Replace "amount" with a value between 0-51. The larger the amount the smaller the file size will be, but quality will be worse. A value of 19 is perceptually lossless, resulting in a smaller file size with about the same quality. Optionally, adding "-bf:v 3 -rc-lookahead:v 32" anywhere between the inputs and outputs may increase the quality when using hardware acceleration. See more about it [here](https://superuser.com/questions/1236275/).
</details>

## Important Notes

The video output, in both `h264` and `mp4` formats, may not be watchable with all programs due to how the video is encoded. [VLC Media Player](https://www.videolan.org/) is a program that can view both if you have trouble, and YouTube has no issue with uploaded `mp4` outputs either.

This is a relatively involved process to render a video, more so than I would like. I am still working on a better solution that will handle all of this processing as part of the mod, however it is a rather difficult problem (at least for me) and so I decided I would rather not delay the whole mod or even just renders indefinitely until then. If this process is too difficult or annoying, you can always wait until I make it better.

# Credits

[NSGolova](https://github.com/NSGolova) for making the BeatLeader format and recorder, buggy as it may be

[Fern](https://github.com/Fernthedev) for creating the majority of Hollywood, the renderer

[Sc2ad](https://github.com/Sc2ad) and the rest of the Quest modding people
