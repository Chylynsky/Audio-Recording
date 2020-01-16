# Audio-Recording
Set of Win32 Mmeapi multithreaded wrappers allowing easy audio recording.

About the project:

Everything here boils down to wrapping Win32 handles and functions into classes that take care of releasing all the 
acquired resources.
Recording takes place on its own thread. When started (after initialization) the thread acquires the mutex and waits until 
one of the two buffers is full, which then fires the callback unlocking it. When unlocked, buffers are swapped and the recorded 
samples are moved into the audio_block - the main buffer holding all the samples until explicitly cleared.
