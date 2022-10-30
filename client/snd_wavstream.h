/* FS: Stuff for music WAV streaming */

void S_WAV_Init (void);
void S_WAV_Shutdown (void);
void S_WAV_Restart (void);
void S_StopWAVBackgroundTrack (void);
void S_StreamWAVBackgroundTrack(void);
void S_StartWAVBackgroundTrack(const char *introTrack, const char *loopTrack);
void S_UpdateWavTrack(void);
