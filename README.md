# Replay

Watch back your best scores!

## FAQ (Frequently Asked Questions)

- How do I record a replay?
  - For now, the only way to record replays is with [BeatLeader](https://github.com/BeatLeader/beatleader-qmod/releases). All you have to do is play any level with BeatLeader installed. Replay will save all replays from BeatLeader, and add a replay button next to the play button for levels that have a replay.
  - Replays from the old 1.17.1 version of this mod are available to view, but cannot be recorded. Recording them will never be added becuase their format is much worse for viewing and rendering.

- What is the purpose of the Render Video and Record Audio features?
  - The Render Video feature allows Quest gameplay to look identical to PC recordings. You can now get extra FOV, mirrors, shockwaves, etc all running smoothly at the framerate you set it to.
   - The Record Audio feature allows for audio quality much higher than what the Quest can record.

- Why is my Quest randomly crashing while rendering?
  - If your Quest isn’t normally crashing either during normal replays or out of replays entirely, this is most likely due to the Quest running out of memory. This can especially be the case if you are playing an NE or Chroma map, or rendering at 4k. Try lowering your video resolution and bitrate. This is not fixable.
  - Alternatively, try rendering one or two more times and/or restarting your Quest. There is a (relatively small) chance it won't crash.

- The gameplay in my headset looks messed up while using the rendering feature (missing hud, trails, ect), what can I do to fix this?
  - This is caused by the Quest being under a lot of stress while rendering. This will not be seen on the finished video.
  - Shockwaves and distortions will always be broken on the headset camera.

- My Quest has gone to sleep while during a render. What effects will this have?
  - If you have Allow Pauses disabled, the render will continue the second the Quest has woken up and the finished video will have no effects.
  - If you have it enabled, the render will have the pause menu in it. If you unpause, it will still continue rendering the level, but you may want to edit it out.

- After mixing the audio with the video, it doesn’t sync up perfectly, how can I fix this?
  - For now this is how it will always be. After mixing the audio, you can fix the audio in the average free video editor.

## Rendering tips

- Rendering will pause if the headset goes to sleep. If you have adb, you can disable the proximity sensor and prevent it from falling asleep with the command `adb shell am broadcast -a com.oculus.vrpowermanager.prox_close`. It will be enabled again if the headset restarts, or if you run the command `adb shell am broadcast -a com.oculus.vrpowermanager.automation_disable`.

- Rendering at a resolution higher than 1080p may cause Beat Saber to run out of memory and crash.

- Closing the game during a render will most likely cause the output to be corrupt. If you want to stop it before the level is finished, enable "Allow Pauses" in the rendering section of the mod settings before starting the render and exit through the pause menu.

## Processing renders

After rendering a video using the mod, a file will be created (or a previously existing file overwritten) called `video.h264` in the top level directory of your Quest. Recording audio will create a similar file named `audio.wav`, or alternatively you can use the audio file from the map itself instead of recording if you are fine without having hitsounds. You will want to pull both a video file and an audio file to your PC.

Next, you need to have [ffmpeg](https://ffmpeg.org/) installed. With that, running the command `ffmpeg -i "path/to/video.h264" -i "path/to/audio.wav" "path/to/output/video.mp4"` will churn for a while and produce a `mp4` file (or another format if you specify) which can be watched or uploaded to YouTube as desired.

### Important Notes

The video output, in both `h264` and `mp4` formats, may not be watchable with all programs due to how the video is encoded. [VLC Media Player](https://www.videolan.org/) is a program that can view both if you have trouble, and YouTube has no issue with uploaded `mp4`s either.

This is a relatively involved process to render a video, more so than I would like. I am still working on a better solution that will handle all of this processing as part of the mod, however it is a rather difficult problem (at least for me) and so I decided I would rather not delay the whole mod or even just renders indefinitely until then. If this process is too difficult or annoying, you can always wait until I make it better.

# Credits

[NSGolova](https://github.com/NSGolova) for making the BeatLeader format and recorder, buggy as it may be

[Fern](https://github.com/Fernthedev) for creating the majority of Hollywood, the renderer

[Sc2ad](https://github.com/Sc2ad) and the rest of the Quest modding people
