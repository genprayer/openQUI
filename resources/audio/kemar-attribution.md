# KEMAR impulse responses

Copyright 1994 MIT Media Laboratory. Measurements by Bill Gardner and Keith
Martin, MIT Media Lab. The authors allow unrestricted use provided they are
cited in research and commercial applications.

Source: https://sound.media.mit.edu/resources/KEMAR.html
Archive: https://sound.media.mit.edu/resources/KEMAR/diffuse.zip
Archive SHA-256: `a48e73137390626ef1e45d1de6a4d98530c6aa570e71b40f7c4442241ed5fa22`.

`kemar-7.1.wav` repacks their 44.1 kHz, 128-sample, diffuse-field equalized
measurements at elevation 0 and azimuths 0, 30, 90 and 150 degrees. Left
directions mirror the two ear responses. No resampling, invented impulses or
proprietary Dolby/Nahimic data are used. Each adjacent pair represents the two
ears, in input order FL, FR, FC, LFE, RL, RR, SL, SR. The LFE pair repeats FC;
the filter graph low-passes that input before convolution.

Reproduction: `python3 tools/audio/build_kemar.py diffuse.zip resources/audio/kemar-7.1.wav`.
This measured dummy-head response is not an individual HRTF or an emulation
of Corsair's Windows audio DSP.
