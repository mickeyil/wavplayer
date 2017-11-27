#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <ctime>

#include <soundio/soundio.h>
#include	<sndfile.hh>

void sleep_ms(int milliseconds);


// This pointer holds the buffer which will contain the whole
// WAV data.
// The data is stored as S16LE (singed integer 16 bits, little endian)
// Which means that it basically looks like this:
//
// byte index:                 01 23 45 67 89
//                             L |R |L |R |L |R ...
// uint16 (short) index (pos): 0  1  2  3  4
// frame index                 0     1     2

short *buffer = NULL;

// global position uint16 pointer for input data buffer
size_t pos = 0;

// number of frames already copied to the sound buffer
int frames_copied = 0;

// total number of frames for playback
int playback_frames = 0;

// signals that the sound buffer is empty - nothing to playback
static volatile bool done_playing = false;


static void underflow_callback(struct SoundIoOutStream *outstream) {
    static int count = 0;
    fprintf(stderr, "underflow %d\n", count++);

    int frames_remaining = playback_frames - frames_copied;
    if (frames_remaining == 0) {
        done_playing = true;
    } else {
      fprintf(stderr, "read underrun occured!!\n");
      exit(1);
    }
    printf("exit underflow callback\n");
}

static void write_callback(struct SoundIoOutStream *outstream,
        int frame_count_min, int frame_count_max)
{
    const struct SoundIoChannelLayout *layout = &outstream->layout;
    const int channels = layout->channel_count;
    struct SoundIoChannelArea *areas;

    if (done_playing) {
      printf("done playing. returning from callback.\n");
      return;
    }

    double next_frame_latency = 0.0;
    int err;
    printf("query latency\n");
    if ((err = soundio_outstream_get_latency(outstream, &next_frame_latency))) {
        fprintf(stderr, "%s\n", soundio_strerror(err));
        exit(1);
    }

    double playtime = frames_copied / 44100.0 - next_frame_latency;

    printf("frames copied: %d, playtime: %.3lf, next frame latency: %.3lf, frame_count_max: %d\n", 
      frames_copied, playtime, next_frame_latency, frame_count_max);
    
    int frames_remaining = playback_frames - frames_copied;

    if (frames_remaining == 0 && next_frame_latency == 0.0) {
      printf("** done playing (write callback) **\n");
         fprintf(stderr, "pausing result: %s\n",
         soundio_strerror(soundio_outstream_pause(outstream, true)));
         done_playing = true;
    }

    int frames_to_copy = std::min(frames_remaining, frame_count_max);
    
    if (frames_to_copy <= 0) {
      printf("no more data. frames_to_copy = %d\n", frames_to_copy);
      return;
    }


    while (frames_to_copy > 0) {
        int frame_count = frames_to_copy;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            fprintf(stderr, "%s\n", soundio_strerror(err));
            exit(1);
        }

        if (!frame_count)
            break;
        
        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int channel = 0; channel < channels; channel += 1) {
                
                short *ptr = (short*)(areas[channel].ptr + areas[channel].step * frame);
                
                // hard coded: 2 number of channels
                *ptr = buffer[pos + 2*frame + channel];
            }
        }


        if ((err = soundio_outstream_end_write(outstream))) {
            fprintf(stderr, "%s\n", soundio_strerror(err));
            exit(1);
        }

        // update position buffer with the amount of frames played
        pos += channels * frame_count;

        frames_copied += frame_count;
        frames_to_copy -= frame_count;
    }
}



int main(int argc, char **argv) {

    setbuf(stdout, NULL);
		if (argc <= 1) {
        printf("Usage: %s file.wav\n", argv[0]);
        exit(1);
    }

    const char * input_filename = argv[1];
    printf("playing: %s\n", input_filename);

    int err;
    struct SoundIo *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    if ((err = soundio_connect_backend(soundio, SoundIoBackendAlsa))) {
        fprintf(stderr, "error connecting: %s\n", soundio_strerror(err));
        return 1;
    }

    fprintf(stderr, "Backend: %s\n", soundio_backend_name(soundio->current_backend));

    soundio_flush_events(soundio);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) {
        fprintf(stderr, "no output device found\n");
        return 1;
    }

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    fprintf(stderr, "Output device: %s\n", device->name);

    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    if (!outstream) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    
    // pre load WAV data into a buffer
    SndfileHandle file = SndfileHandle (input_filename);
					
    playback_frames = file.frames();
    
    printf ("Opened file '%s'\n", input_filename) ;
    printf ("    Sample rate : %d\n", file.samplerate ()) ;
    printf ("    Channels    : %d\n", file.channels ()) ;
    printf ("    Frames      : %lu\n", (uint64_t) file.frames ()) ;
    const int bytes_per_sample = 2;

    size_t buf_size = file.frames() * file.channels() * bytes_per_sample;
    printf("buffer size: %zu\n", buf_size);

    buffer = (short*) malloc (buf_size);
    sf_count_t n_read = file.readRaw (buffer, buf_size);
    printf("n_read = %lu\n", (uint64_t) n_read);

    outstream->format = SoundIoFormatS16LE;
    outstream->sample_rate = 44100;
    outstream->write_callback = write_callback;
    outstream->underflow_callback = underflow_callback;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
        return 1;
    }

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
        return 1;
    }

    for (;;) {
        soundio_flush_events(soundio);
        sleep_ms(100);
        printf("main loop\n");
        if (done_playing) {
          printf("done.\n");
          break;
        }
    }
    printf("calling outstream destroy\n");
    soundio_outstream_destroy(outstream);
    printf("calling device unref\n");
    soundio_device_unref(device);
    printf("calling destroy\n");
    soundio_destroy(soundio);
    free(buffer);
    for (;;) {
      printf("***\n");
      sleep_ms(100);
    }
    return 0;
}



void sleep_ms(int milliseconds)
{
  	struct timespec ts;
  	ts.tv_sec = milliseconds / 1000;
  	ts.tv_nsec = (milliseconds % 1000) * 1000000;
  	nanosleep(&ts, NULL);
}
