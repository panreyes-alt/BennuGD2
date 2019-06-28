/*
 *  Copyright (C) 2006-2019 SplinterGU (Fenix/BennuGD)
 *  Copyright (C) 2002-2006 Fenix Team (Fenix)
 *  Copyright (C) 1999-2002 José Luis Cebrián Pagüe (Fenix)
 *
 *  This file is part of Bennu Game Development
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bgddl.h"

#include <SDL.h>

#include "SDL_mixer.h"

#include "files.h"
#include "xstrings.h"

#include "dlvaracc.h"

#include "bgload.h"

/* --------------------------------------------------------------------------- */

static int audio_initialized = 0 ;

/* --------------------------------------------------------------------------- */

#define SOUND_FREQ              0
#define SOUND_MODE              1
#define SOUND_CHANNELS          2

/* --------------------------------------------------------------------------- */
/* Son las variables que se desea acceder.                                     */
/* El interprete completa esta estructura, si la variable existe.              */
/* (usada en tiempo de ejecucion)                                              */

DLVARFIXUP  __bgdexport( libmod_sound, globals_fixup )[] =
{
    /* Nombre de variable global, puntero al dato, tamano del elemento, cantidad de elementos */
    { "sound.sound_freq"        , NULL, -1, -1 },
    { "sound.sound_mode"        , NULL, -1, -1 },
    { "sound.sound_channels"    , NULL, -1, -1 },
    { NULL                      , NULL, -1, -1 }
};

/* ------------------------------------- */
/* Interfaz SDL_RWops Bennu              */
/* ------------------------------------- */

static Sint64 SDLCALL __libmod_sound_seek_cb( SDL_RWops *context, Sint64 offset, int whence ) {
    if ( file_seek( context->hidden.unknown.data1, offset, whence ) < 0 ) return ( -1 );
    return( file_pos( context->hidden.unknown.data1 ) );
//    return ( file_seek( context->hidden.unknown.data1, offset, whence ) );
}

static size_t SDLCALL __libmod_sound_read_cb( SDL_RWops *context, void *ptr, size_t size, size_t maxnum ) {
    int ret = file_read( context->hidden.unknown.data1, ptr, size * maxnum );
    if ( ret > 0 ) ret /= size;
    return( ret );
}

static size_t SDLCALL __libmod_sound_write_cb( SDL_RWops *context, const void *ptr, size_t size, size_t num ) {
    int ret = file_write( context->hidden.unknown.data1, ( void * )ptr, size * num );
    if ( ret > 0 ) ret /= size;
    return( ret );
}

static int SDLCALL __libmod_sound_close_cb( SDL_RWops *context ) {
    if ( context ) {
        file_close( context->hidden.unknown.data1 );
        SDL_FreeRW( context );
    }
    return( 0 );
}

static SDL_RWops *SDL_RWFromBGDFP( file *fp ) {
    SDL_RWops *rwops = SDL_AllocRW();
    if ( rwops != NULL ) {
        rwops->seek = __libmod_sound_seek_cb;
        rwops->read = __libmod_sound_read_cb;
        rwops->write = __libmod_sound_write_cb;
        rwops->close = __libmod_sound_close_cb;
        rwops->hidden.unknown.data1 = fp;
    }
    return( rwops );
}

/* --------------------------------------------------------------------------- */

/*
 *  FUNCTION : sound_init
 *
 *  Set the SDL_Mixer library
 *
 *  PARAMS:
 *      no params
 *
 *  RETURN VALUE:
 *
 *  no return
 *
 */

static int sound_init() {
    int audio_rate;
    Uint16 audio_format;
    int audio_channels;
    int audio_buffers;
    int audio_mix_channels;

    if ( !audio_initialized ) {
        /* Initialize variables: but limit quality to some fixed options */

        audio_rate = GLOQWORD( libmod_sound, SOUND_FREQ );

        if ( audio_rate > 22050 )       audio_rate = 44100;
        else if ( audio_rate > 11025 )  audio_rate = 22050;
        else                            audio_rate = 11025;

        audio_format = AUDIO_S16;
        audio_channels = GLOQWORD( libmod_sound, SOUND_MODE ) + 1;
        audio_buffers = 1024 * audio_rate / 22050;

        /* Open the audio device */
        if ( Mix_OpenAudio( audio_rate, audio_format, audio_channels, audio_buffers ) >= 0 ) {
            GLOQWORD( libmod_sound, SOUND_CHANNELS ) <= 32 ? Mix_AllocateChannels( GLOQWORD( libmod_sound, SOUND_CHANNELS ) ) : Mix_AllocateChannels( 32 ) ;
            Mix_QuerySpec( &audio_rate, &audio_format, &audio_channels );
            audio_mix_channels = Mix_AllocateChannels( -1 ) ;
            GLOQWORD( libmod_sound, SOUND_CHANNELS ) = audio_mix_channels ;

            audio_initialized = 1;
            return 0;
        }
    }

    fprintf( stderr, "[SOUND] No se pudo inicializar el audio: %s\n", SDL_GetError() ) ;
    audio_initialized = 0;
    return -1 ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : sound_close
 *
 *  Close all the audio set
 *
 *  PARAMS:
 *      no params
 *
 *  RETURN VALUE:
 *
 *  no return
 *
 */

static void sound_close() {
    if ( !audio_initialized ) return;

    //falta por comprobar que todo esté descargado

    Mix_CloseAudio();

    audio_initialized = 0;
}


/* ------------------ */
/* Sonido MOD y OGG   */
/* ------------------ */

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : load_song
 *
 *  Load a MOD/OGG from a file
 *
 *  PARAMS:
 *      file name
 *
 *  RETURN VALUE:
 *
 *  mod pointer
 *
 */

static int64_t load_song( const char * filename ) {
    Mix_Music *music = NULL;
    file      *fp;

    if ( !audio_initialized && sound_init() ) return ( 0 );

    if ( !( fp = file_open( filename, "rb0" ) ) ) return ( 0 );

    if ( !( music = Mix_LoadMUS_RW( SDL_RWFromBGDFP( fp ), 0 ) ) ) {
        file_close( fp );
        fprintf( stderr, "Couldn't load %s: %s\n", filename, SDL_GetError() );
        return( 0 );
    }

    return (( int64_t )( intptr_t )music );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : play_song
 *
 *  Play a MOD/OGG
 *
 *  PARAMS:
 *      mod pointer
 *      number of loops (-1 infinite loops)
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int play_song( int64_t id, int loops ) {
    if ( audio_initialized && id ) {
        int result = Mix_PlayMusic(( Mix_Music * )( intptr_t )id, loops );
        if ( result == -1 ) fprintf( stderr, "%s", Mix_GetError() );
        return result;
    }
    fprintf( stderr, "Play song called with invalid handle" );
    return( -1 );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : fade_music_in
 *
 *  Play a MOD/OGG fading in it
 *
 *  PARAMS:
 *      mod pointer
 *      number of loops (-1 infinite loops)
 *      ms  microsends of fadding
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int fade_music_in( int64_t id, int loops, int ms ) {
    if ( audio_initialized && id ) return( Mix_FadeInMusic(( Mix_Music * )( intptr_t )id, loops, ms ) );
    return( -1 );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : fade_music_off
 *
 *  Stop the play of a mod
 *
 *  PARAMS:
 *
 *  ms  microsends of fadding
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int fade_music_off( int ms ) {
    if ( !audio_initialized ) return ( 0 );
    return ( Mix_FadeOutMusic( ms ) );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : unload_song
 *
 *  Play a MOD
 *
 *  PARAMS:
 *
 *  mod id
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int unload_song( int64_t id ) {
    if ( audio_initialized && id ) {
        if ( Mix_PlayingMusic() ) Mix_HaltMusic();
        Mix_FreeMusic(( Mix_Music * )( intptr_t )id );
    }
    return ( 0 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : stop_song
 *
 *  Stop the play of a mod
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int stop_song( void ) {
    if ( audio_initialized ) Mix_HaltMusic();
    return ( 0 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : pause_song
 *
 *  Pause the mod in curse, you can resume it after
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int pause_song( void ) {
    if ( audio_initialized ) Mix_PauseMusic();
    return ( 0 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : resume_song
 *
 *  Resume the mod, paused before
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int resume_song( void ) {
    if ( audio_initialized ) Mix_ResumeMusic();
    return( 0 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : is_playing_song
 *
 *  Check if there is any mod playing
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *  TRUE OR FALSE if there is no error
 *
 */

static int is_playing_song( void ) {
    if ( !audio_initialized ) return ( 0 );
    return Mix_PlayingMusic();
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : set_song_volume
 *
 *  Set the volume for mod playing (0-128)
 *
 *  PARAMS:
 *
 *  int volume
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *  0 if there is no error
 *
 */

static int set_song_volume( int volume ) {
    if ( !audio_initialized && sound_init() ) return ( -1 );

    if ( volume < 0 ) volume = 0;
    if ( volume > 128 ) volume = 128;

    Mix_VolumeMusic( volume );
    return 0;
}

/* ------------ */
/* Sonido WAV   */
/* ------------ */

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : load_wav
 *
 *  Load a WAV from a file
 *
 *  PARAMS:
 *      file name
 *
 *  RETURN VALUE:
 *
 *  wav pointer
 *
 */

static int64_t load_wav( const char * filename ) {
    Mix_Chunk *music = NULL;
    file      *fp;

    if ( !audio_initialized && sound_init() ) return ( 0 );

    if ( !( fp = file_open( filename, "rb0" ) ) ) return ( 0 );

    if ( !( music = Mix_LoadWAV_RW( SDL_RWFromBGDFP( fp ), 1 ) ) ) {
        file_close( fp );
        fprintf( stderr, "Couldn't load %s: %s\n", filename, SDL_GetError() );
        return( 0 );
    }
    return (( int64_t )( intptr_t )music );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : play_wav
 *
 *  Play a WAV
 *
 *  PARAMS:
 *      wav pointer;
 *      number of loops (-1 infinite loops)
 *      channel (-1 any channel)
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *  else channel where the music plays
 *
 */

static int play_wav( int64_t id, int loops, int channel ) {
    if ( audio_initialized && id ) return ( ( int ) Mix_PlayChannel( channel, ( Mix_Chunk * )( intptr_t )id, loops ) );
    return ( -1 );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : unload_wav
 *
 *  Frees the resources from a wav, unloading it
 *
 *  PARAMS:
 *
 *  wav pointer
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int unload_wav( int64_t id ) {
    if ( audio_initialized && id ) Mix_FreeChunk(( Mix_Chunk * )( intptr_t )id );
    return ( 0 );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : stop_wav
 *
 *  Stop a wav playing
 *
 *  PARAMS:
 *
 *  int channel
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int stop_wav( int canal ) {
    if ( audio_initialized && Mix_Playing( canal ) ) return( Mix_HaltChannel( canal ) );
    return ( -1 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : pause_wav
 *
 *  Pause a wav playing, you can resume it after
 *
 *  PARAMS:
 *
 *  int channel
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int pause_wav( int canal ) {
    if ( audio_initialized && Mix_Playing( canal ) ) {
        Mix_Pause( canal );
        return ( 0 ) ;
    }
    return ( -1 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : resume_wav
 *
 *  Resume a wav playing, paused before
 *
 *  PARAMS:
 *
 *  int channel
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int resume_wav( int canal ) {
    if ( audio_initialized && Mix_Playing( canal ) ) {
        Mix_Resume( canal );
        return ( 0 ) ;
    }
    return ( -1 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : is_playing_wav
 *
 *  Check a wav playing
 *
 *  PARAMS:
 *
 *  int channel
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *  TRUE OR FALSE if there is no error
 *
 */

static int is_playing_wav( int canal ) {
    if ( audio_initialized ) return( Mix_Playing( canal ) );
    return ( 0 );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : set_wav_volume
 *
 *  Set the volume for wav playing (0-128) IN SAMPLE
 *
 *  PARAMS:
 *
 *  channel id
 *  int volume
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int  set_wav_volume( int64_t sample, int volume ) {
    if ( !audio_initialized ) return ( -1 );

    if ( volume < 0 ) volume = 0;
    if ( volume > 128 ) volume = 128;

    if ( sample ) return( Mix_VolumeChunk(( Mix_Chunk * )( intptr_t )sample, volume ) );

    return -1 ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : set_channel_volume
 *
 *  Set the volume for wav playing (0-128) IN CHANNEL
 *
 *  PARAMS:
 *
 *  channel id
 *  int volume
 *
 *  RETURN VALUE:
 *
 * -1 if there is any error
 *
 */

static int set_channel_volume( int canal, int volume ) {
    if ( !audio_initialized && sound_init() ) return ( -1 );

    if ( volume < 0 ) volume = 0;
    if ( volume > 128 ) volume = 128;

    return( Mix_Volume( canal, volume ) );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : reserve_channels
 *
 *  Reserve the first channels (0 -> n-1) for the application, i.e. don't allocate
 *  them dynamically to the next sample if requested with a -1 value below.
 *
 *  PARAMS:
 *  number of channels to reserve.
 *
 *  RETURN VALUE:
 *  number of reserved channels.
 * -1 if there is any error
 *
 */

static int reserve_channels( int canales ) {
    if ( !audio_initialized && sound_init() ) return ( -1 );
    return Mix_ReserveChannels( canales );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : set_panning
 *
 *  Set the panning for a wav channel
 *
 *  PARAMS:
 *
 *  channel
 *  left volume (0-255)
 *  right volume (0-255)
 *
 */

static int set_panning( int canal, int left, int right ) {
    if ( !audio_initialized && sound_init() ) return ( -1 );

    if ( Mix_Playing( canal ) ) {
        Mix_SetPanning( canal, ( Uint8 )left, ( Uint8 )right );
        return ( 0 ) ;
    }
    return ( -1 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : set_position
 *
 *  Set the position of a channel. (angle) is an integer from 0 to 360
 *
 *  PARAMS:
 *
 *  channel
 *  angle (0-360)
 *  distance (0-255)
 *
 */

static int set_position( int canal, int angle, int dist ) {
    if ( !audio_initialized && sound_init() ) return ( -1 );

    if ( Mix_Playing( canal ) ) {
        Mix_SetPosition( canal, ( Sint16 )angle, ( Uint8 )dist );
        return ( 0 ) ;
    }
    return ( -1 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : set_distance
 *
 *  Set the "distance" of a channel. (distance) is an integer from 0 to 255
 *  that specifies the location of the sound in relation to the listener.
 *
 *  PARAMS:
 *
 *  channel
 *
 *  distance (0-255)
 *
 */

static int set_distance( int canal, int dist ) {
    if ( !audio_initialized && sound_init() ) return ( -1 );

    if ( Mix_Playing( canal ) ) {
        Mix_SetDistance( canal, ( Uint8 )dist );
        return ( 0 ) ;
    }

    return ( -1 ) ;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : reverse_stereo
 *
 *  Causes a channel to reverse its stereo.
 *
 *  PARAMS:
 *
 *  channel
 *  flip  0 normal  != reverse
 *
 */

static int reverse_stereo( int canal, int flip ) {
    if ( !audio_initialized && sound_init() ) return ( -1 );

    if ( Mix_Playing( canal ) ) {
        Mix_SetReverseStereo( canal, flip );
        return ( 0 ) ;
    }

    return ( -1 ) ;
}

/* --------------------------------------------------------------------------- */
/* Sonido                                                                      */
/* --------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_load_song
 *
 *  Load a MOD from a file
 *
 *  PARAMS:
 *      file name
 *
 *  RETURN VALUE:
 *
 *      mod id
 *
 */

static int64_t libmod_sound_load_song( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    int64_t var;
    const char * filename ;

    if ( !( filename = string_get( params[0] ) ) ) return ( 0 ) ;

    var = load_song( filename );
    string_discard( params[0] );

    return ( var );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_load_song2
 *
 *  Load a MOD from a file
 *
 *  PARAMS:
 *      file name
 *      pointer mod id
 *
 *  RETURN VALUE:
 *
 *
 */

static int64_t libmod_sound_bgload_song( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    bgload( load_song, params );
#else
    *(int64_t *)(intptr_t)(params[1]) = -1;
#endif
    return 0;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_play_song
 *
 *  Play a MOD
 *
 *  PARAMS:
 *      mod id;
 *      number of loops (-1 infinite loops)
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_play_song( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    if ( params[0] == -1LL ) return -1; // check for !params[0] in internal function
    return( play_song( params[0], params[1] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_unload_song
 *
 *  Frees the resources from a MOD and unloads it
 *
 *  PARAMS:
 *      mod id;
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_unload_song( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    if ( params[0] == -1LL ) return -1; // check for !params[0] in internal function
    return( unload_song( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_unload_song2
 *
 *  Frees the resources from a MOD and unloads it
 *
 *  PARAMS:
 *      mod *id;
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_unload_song2( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    int64_t *s = (int64_t *)(intptr_t)(params[0]), r;
    if ( !s || *s == -1LL ) return -1; // check for !*s in internal function
    r = unload_song( *s );
    *s = 0LL;
    return( r );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_stop_song
 *
 *  Stop the play of a mod
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_stop_song( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( stop_song() );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_pause_song
 *
 *  Pause the mod in curse, you can resume it after
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_pause_song( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( pause_song() );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_resume_song
 *
 *  Resume the mod, paused before
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_resume_song( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( resume_song() );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_is_playing_song
 *
 *  Check if there is any mod playing
 *
 *  PARAMS:
 *
 *  no params
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  TRUE OR FALSE if there is no error
 *
 */

static int64_t libmod_sound_is_playing_song( INSTANCE * my, int64_t * params ) {
    return ( is_playing_song() );
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_set_song_volume
 *
 *  Set the volume for mod playing (0-128)
 *
 *  PARAMS:
 *
 *  int volume
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if there is no error
 *
 */

static int64_t libmod_sound_set_song_volume( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return ( set_song_volume( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_fade_music_in
 *
 *  Play a MOD/OGG fading in it
 *
 *  PARAMS:
 *      mod pointer
 *      number of loops (-1 infinite loops)
 *      ms  microsends of fadding
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *
 */

static int64_t libmod_sound_fade_music_in( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    if ( params[0] == -1LL ) return -1; // check for !params[0] in internal function
    return ( fade_music_in( params[0], params[1], params[2] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_fade_music_off
 *
 *  Stop the play of a mod
 *
 *  PARAMS:
 *
 *  ms  microsends of fadding
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *
 */

static int64_t libmod_sound_fade_music_off( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return ( fade_music_off( params[0] ) );
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_load_wav
 *
 *  Load a WAV from a file
 *
 *  PARAMS:
 *      file name
 *
 *  RETURN VALUE:
 *
 *      wav id
 *
 */

static int64_t libmod_sound_load_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    int64_t var;
    const char * filename ;

    if ( !( filename = string_get( params[0] ) ) ) return ( 0 ) ;

    var = load_wav( filename );
    string_discard( params[0] );

    return ( var );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_load_wav2
 *
 *  Load a WAV from a file
 *
 *  PARAMS:
 *      file name
 *      pointer wav id
 *
 *  RETURN VALUE:
 *
 *
 */

static int64_t libmod_sound_bgload_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    bgload( load_wav, params );
#else
    *(int64_t *)(intptr_t)(params[1]) = -1;
#endif
    return 0;
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_play_wav
 *
 *  Play a WAV
 *
 *  PARAMS:
 *      wav id;
 *      number of loops (-1 infinite loops)
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_play_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    if ( params[0] == -1LL ) return -1; // check for !params[0] in internal function
    return( play_wav( params[0], params[1], -1 ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_play_wav_channel
 *
 *  Play a WAV
 *
 *  PARAMS:
 *      wav id;
 *      number of loops (-1 infinite loops)
 *      channel (-1 like libmod_sound_play_wav)
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_play_wav_channel( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    if ( params[0] == -1LL ) return -1; // check for !params[0] in internal function
    return( play_wav( params[0], params[1], params[2] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_unload_wav
 *
 *  Frees the resources from a wav, unloading it
 *
 *  PARAMS:
 *
 *  mod id
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_unload_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    if ( params[0] == -1LL ) return -1; // check for !params[0] in internal function
    return( unload_wav( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_unload_wav2
 *
 *  Frees the resources from a wav, unloading it
 *
 *  PARAMS:
 *
 *  mod *id
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_unload_wav2( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    int64_t *s = (int64_t *)(intptr_t)(params[0]), r;
    if ( !s || *s == -1LL ) return -1; // check for !*s in internal function
    r = unload_wav( *s );
    *s = 0LL;
    return( r );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_stop_wav
 *
 *  Stop a wav playing
 *
 *  PARAMS:
 *
 *  channel
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_stop_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( stop_wav( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_pause_wav
 *
 *  Pause a wav playing, you can resume it after
 *
 *  PARAMS:
 *
 *  channel
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_pause_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return ( pause_wav( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : resume_wav
 *
 *  Resume a wav playing, paused before
 *
 *  PARAMS:
 *
 *  channel
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if all goes ok
 *
 */

static int64_t libmod_sound_resume_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return ( resume_wav( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : is_playing_wav
 *
 *  Check a wav playing
 *
 *  PARAMS:
 *
 *  channel
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  TRUE OR FALSE if there is no error
 *
 */


static int64_t libmod_sound_is_playing_wav( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return ( is_playing_wav( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_set_channel_volume
 *
 *  Set the volume for a wav playing (0-128)
 *
 *  PARAMS:
 *
 *  channel
 *  int volume
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if there is no error
 *
 */

static int64_t libmod_sound_set_channel_volume( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( set_channel_volume( params[0], params[1] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_reserve_channels
 *
 *  Reserve the first channels (0 -> n-1) for the application, i.e. don't allocate
 *  them dynamically to the next sample if requested with a -1 value below.
 *
 *  PARAMS:
 *  number of channels to reserve.
 *
 *  RETURN VALUE:
 *  number of reserved channels.
 *  -1 if there is any error
 *
 */

static int64_t libmod_sound_reserve_channels( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return ( reserve_channels( params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_set_wav_volume
 *
 *  Set the volume for a wav playing (0-128)
 *
 *  PARAMS:
 *
 *  channel
 *  int volume
 *
 *  RETURN VALUE:
 *
 *  -1 if there is any error
 *  0 if there is no error
 *
 */

static int64_t libmod_sound_set_wav_volume( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( set_wav_volume( params[0], params[1] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_set_panning
 *
 *  Set the panning for a wav channel
 *
 *  PARAMS:
 *
 *  channel
 *  left volume (0-255)
 *  right volume (0-255)
 *
 */

static int64_t libmod_sound_set_panning( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( set_panning( params[0], params[1], params[2] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_set_position
 *
 *  Set the position of a channel. (angle) is an integer from 0 to 360
 *
 *  PARAMS:
 *
 *  channel
 *  angle (0-360)
 *  distance (0-255)
 *
 */

static int64_t libmod_sound_set_position( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( set_position( params[0], params[1], params[2] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_set_distance
 *
 *  Set the "distance" of a channel. (distance) is an integer from 0 to 255
 *  that specifies the location of the sound in relation to the listener.
 *
 *  PARAMS:
 *
 *  channel
 *
 *  distance (0-255)
 *
 */

static int64_t libmod_sound_set_distance( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( set_distance( params[0], params[1] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/*
 *  FUNCTION : libmod_sound_reverse_stereo
 *
 *  Causes a channel to reverse its stereo.
 *
 *  PARAMS:
 *
 *  channel
 *
 *  flip 0 normal != reverse
 *
 */

static int64_t libmod_sound_reverse_stereo( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( reverse_stereo( params[0], params[1] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */

static int64_t libmod_sound_set_music_position( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return ( Mix_SetMusicPosition( *( double * ) &params[0] ) );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */

static int64_t libmod_sound_init( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    return( sound_init() );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */

static int64_t libmod_sound_close( INSTANCE * my, int64_t * params ) {
#ifndef TARGET_DINGUX_A320
    sound_close();
    return( 0 );
#else
    return -1;
#endif
}

/* --------------------------------------------------------------------------- */
/* Funciones de inicializacion del modulo/plugin                               */

void  __bgdexport( libmod_sound, module_initialize )() {
#ifndef TARGET_DINGUX_A320
    if ( !SDL_WasInit( SDL_INIT_AUDIO ) ) SDL_InitSubSystem( SDL_INIT_AUDIO );
#endif
}

/* --------------------------------------------------------------------------- */

void __bgdexport( libmod_sound, module_finalize )() {
#ifndef TARGET_DINGUX_A320
    if ( SDL_WasInit( SDL_INIT_AUDIO ) ) SDL_QuitSubSystem( SDL_INIT_AUDIO );
#endif
}

/* ----------------------------------------------------------------- */
/* exports                                                           */
/* ----------------------------------------------------------------- */

#include "libmod_sound_exports.h"

/* ----------------------------------------------------------------- */
