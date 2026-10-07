#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the Wii queue against rejected refills and asynchronous consumption."""

from pathlib import Path
import subprocess
import tempfile
import atexit
import shutil
import os

os.chdir(Path(__file__).resolve().parents[1])
p = Path(tempfile.mkdtemp(prefix='paperplane-audio-queue-'))
atexit.register(shutil.rmtree, p)
s = Path('source/platform_wii.c').read_text()
impl = s[s.index('static void audio_service(Audio*a)') : s.index('static void *audio_producer')]
impl += s[s.index('void platform_audio_reset(') : s.index('uint64_t platform_audio_played(')]
impl += s[s.index('uint64_t platform_audio_played(') : s.index('void platform_capture(')]
h = '''#include "audio.h"
#include <deque>
#include <cassert>
#include <cstdio>
#define SND_OK 0
#define SND_UNUSED 0
#define VOICE_STEREO_16BIT 1
struct Chunk {short *p;unsigned at;};static std::deque<Chunk> queue;
static short audio_buffers[3][2048];static int audio_started,audio_pending=-1;static uint64_t audio_completed,audio_segment_frames;
static Audio*audio_source;void audio_lock(Audio*){}void audio_unlock(Audio*){}
static unsigned generated,consumed,rejected,adds,segment,clocked;
void DCFlushRange(void*,unsigned){}
void audio_mix(Audio*a,short*out,unsigned n){for(unsigned i=0;i<n;i++){out[2*i]=out[2*i+1]=(generated+i)%30000;}generated+=n;a->mixed_frames+=n;}
int ASND_StatusVoice(int){return queue.empty()?SND_UNUSED:1;}
int ASND_TestVoiceBufferReady(int){return queue.size()<2;}
int ASND_TestPointer(int,void*p){for(auto &c:queue)if(c.p==p)return 1;return 0;}
int ASND_SetVoice(int,int,int,int,void*p,int,int,int,void*){assert(queue.empty());queue.push_back({(short*)p,0});segment=0;return 0;}
int ASND_AddVoice(int,void*p,int){if(adds++%9==0){rejected++;return -1;}assert(queue.size()<2);queue.push_back({(short*)p,0});return 0;}
int ASND_StopVoice(int){queue.clear();return 0;}
unsigned ASND_GetTickCounterVoice(int){return segment*3/2;}
unsigned ASND_GetAudioRate(){return 48000;}
'''
f = '''int main(){Audio a={};platform_audio_reset(&a);for(int step=0;step<300;step++){audio_source=&a;audio_service(&a);for(int i=0;i<512&&!queue.empty();i++){auto &c=queue.front();assert(c.p[2*c.at]==consumed%30000);assert(c.p[2*c.at+1]==consumed%30000);consumed++;segment++;if(++c.at==1024)queue.pop_front();}assert(platform_audio_played()<=a.mixed_frames);}assert(rejected>10&&consumed>100000);printf("PASS: %u ordered output frames despite %u rejected refills; no dropped/repeated samples\\n",consumed,rejected);platform_audio_reset(&a);assert(a.mixed_frames==0&&a.ui_end_frame==0&&platform_audio_played()==0&&audio_pending==-1);}
'''
(p / 'wii-queue.cpp').write_text(h + impl + f)
subprocess.run(
    ['g++', '-O2', '-Isource', str(p / 'wii-queue.cpp'), '-o', str(p / 'wii-queue')], check=True
)
subprocess.run([str(p / 'wii-queue')], check=True)
