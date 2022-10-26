# Replay

Watch back your best scores!

## Processing recordings

After recording a video using the mod, a file will be created (or a previously existing file overwritten) called `video.h264` in the top level directory of your quest. Recording audio will create a similar file named `audio.wav`, or alternatively you can use the audio file from the map itself instead of recording if you are fine without having hitsounds. You will want to pull both a video file and an audio file to your PC.

Next, you need to have [ffmpeg](https://ffmpeg.org/) installed. With that, running the command `ffmpeg -i "path/to/video.h264" -i "path/to/audio.wav" "path/to/output/video.mp4"` will churn for a while and produce a `mp4` file (or another format if you specify) which can be watched or uploaded to YouTube as desired.

### Important Notes

The video output, in both `h264` and `mp4` formats, may not be watchable with all programs due to how the video is encoded. [VLC Media Player](https://www.videolan.org/) is a program that can view both if you have trouble, and YouTube has no issue with uploaded `mp4`s either.

This is a relatively involved process to record a video, more so than I would like. I am still working on a better solution that will handle all of this processing as part of the mod, however it is a rather difficult problem (at least for me) and so I decided I would rather not delay the whole mod or even rendering indefinitely until then. If this process is too difficult or annoying, you can always wait until I make it better.

# Credits

[NSGolova](https://github.com/NSGolova) for making the BeatLeader format and recorder, buggy as it may be

[Fern](https://github.com/Fernthedev) for creating the majority of Hollywood, the renderer

[Sc2ad](https://github.com/Sc2ad) and the rest of the Quest modding people
