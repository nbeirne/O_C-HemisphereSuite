// Copyright (c) 2020, Jason Justian, Nick Beirne
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


// Uses the memory space in OC_patterns to sequence. Like a mini sequins. 
// 4 sequences are played in parallel. The note values may be configured with sequins or an upcoming sequence editor. CV can manipulate the sequences in a variety of ways.
//  
// There are two channels controlled with a single clock (GATE 1). Gate 2 is a reset to makes the internal sequwncer's step equal to 0. 
// Each channel has a few different modes, controllable with the encoder. 
//
// Modes:
// OCT:  you are in this mode when the sequence arrow is solid. The encoder will rotate through sequences and it will play the chosen sequence, with CV controlling the octave (by 1 or 2 octaves). 
// PICK: In this mode CV will pick which sequence is playing. Sampled after each clock.
// QUAN: The internal sequences are ignored and the CV will be quanitzed to a global scale. Right now that's c-dorian and only configurable in code.
// RAND: The CV is ignored, and instead a random sequence is chosen at random.  


class SwitchSeq : public HemisphereApplet {
public:

    const char* applet_name() {
        return "SwitchSeq";
    }

    void Start() {
        ForEachChannel(ch) 
        {
            mode[ch] = -1; // start in PICK mode
        }
        quantizer.Init();
        quantizer.Configure(OC::Scales::GetScale(scale), 0xffff);

    }

    void Controller() {
        // global clock. Do it before reset.
        if (Clock(0)) {
            StartADCLag(0);
        }

        // reset
        if (Clock(1)) {
            step = 0;
        }

        // only make changes on a clock'd signal.
        if (EndOfADCLag(0)) {
            step = (step + 1) % 16;

            ForEachChannel(ch) 
            {
                if (mode[ch] == -3) { // RAND mode
                    cv[ch] = random(0,HEMISPHERE_MAX_CV);
                } else {
                    cv[ch] = In(ch);
                }

                int32_t pitch = ValueForChannel(ch);
                int32_t quantized = quantizer.Process(pitch, root << 7, 0);
                Out(ch, quantized);
            }
        }
    }

    void View() {
        gfxHeader(applet_name());
        // DrawScale();
        DrawMode();
        DrawIndicator();
    }

    void OnButtonPress() {
        if (++cursor > 1) cursor = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        mode[cursor] += direction;
        if (mode[cursor] < -3) mode[cursor] = -3;
        if (mode[cursor] > 3) mode[cursor] = 3;
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0,8}, mode[0] + 32);
        Pack(data, PackLocation {8,8}, mode[1] + 32);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        mode[0] = Unpack(data, PackLocation {0,8}) - 32;
        mode[1] = Unpack(data, PackLocation {8,8}) - 32;
    }

protected:
    // TODO: global scale
    // TODO: help

    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock,2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "CV Ch1,Ch2";
        help[HEMISPHERE_HELP_OUTS]     = "A=Out1,B=Out1";
        help[HEMISPHERE_HELP_ENCODER]  = "Change Mode,Seq";
        //                               "------------------" <-- Size Guide
    }

private:
    // Global Settings
    int root = 0;  // C
    int scale = 7; // DORI
    // TODO: reset vs. 2nd clock mode

    // Modes
    int mode[2]; // 0-4: sequences 0-4 with CV octave transpose (0, 1, or 2 octaves). -1: CV picks sequence, -2 = CV quantize mode, -3 = pick random sequence
    int cv[2]; // 0 -> HEMISPHERE_MAX_CV. RAND mode writes to this (and over-writes input CV) 

    // Sequencer 
    int step = 0; // 0 -> 16
    braids::Quantizer quantizer;

    // UI
    int cursor = 0; 

    void DrawScale() {
        gfxPrint(0, 15, OC::Strings::note_names_unpadded[root]); // we show the scale but can't change it.
        gfxPrint(8, 15, OC::scale_names_short[scale]);
    }

    void DrawMode()
    {
        ForEachChannel(ch) 
        {
            int ypos = 15 + (30*ch);
            if (mode[ch] >= 0) {
                gfxPrint(0, ypos, "OCT");
            } else if (mode[ch] == -1) {
                gfxPrint(0, ypos, "PICK");
            } else if (mode[ch] == -2) {
                gfxPrint(0, ypos, "QUAN");
            } else if (mode[ch] == -3) {
                gfxPrint(0, ypos, "RAND");
            }

            gfxPrintfn(0, ypos+10, 4, "%03d", ValueForChannel(ch) / 128);
            
            // cursor when not picking sequence directly
            if (cursor == ch && mode[ch] < 0) {
                gfxCursor(0, ypos+8, 24);
            }
        }
    }

    void DrawIndicator()
    {
        // position in sequence
        gfxPrintfn(49, 15, 3, "%02d", step+1); 
        
        // step
        for (int i = 0; i < 4; i++) {
            gfxPrintfn(43, 25 + (10*i), 3, "%03d", OC::user_patterns[i].notes[step] / 128);
        }

        // arrow indicators
        ForEachChannel(ch) 
        {
            int seq = SequenceForChannel(ch);
            if (seq >= 0) {
                int x = 28 + (6*ch);
                int y =  25 + (10*seq);
                if (mode[ch] >= 0) {
                    gfxBitmap(x, y, 8, RIGHT_BTN_ICON);
                    // cursor when picking sequence in OCT mode.
                    if (cursor == ch) {
                        gfxCursor(x+3, 63, 5);
                    }
                } else {
                    gfxBitmap(x, y, 8, RIGHT_BTN_ICON_UNFILLED);
                }
            } 
        }
    }

    // uses cv + encoder. -1 if in QUAN mode. 
    int SequenceForChannel(int ch) {
        int seq = 0;
        if (mode[ch] == -2) return -1; // QUAN mode

        if (mode[ch] >= 0)  seq = mode[ch]; // OCT mode
        if (mode[ch] == -1 && cv[ch] > 0) seq = Proportion(Detented(cv[ch]), HEMISPHERE_MAX_CV, 3); // PICK mode
        if (mode[ch] == -3) seq = Proportion(Detented(cv[ch]), HEMISPHERE_MAX_CV, 3); // RAND mode (rand wrote to cv)

        if (seq < 0) seq = 0;
        if (seq > 3) seq = 3;
        return seq; 
    }

    // get pre-quantize CV
    int ValueForChannel(int ch) {
        int seq = SequenceForChannel(ch);
        if (seq >= 0) { 
            int value = OC::user_patterns[seq].notes[step];
            if (mode >= 0) {
                // OCT mode
                value = value + ((12<<7)*Proportion(Detented(cv[ch]), HEMISPHERE_MAX_CV, 2)); // transpose up 1 or 2 octaves. 
            }
            return value;
        } else { // QUAN mode
            int max_cv = 8340; // expects 0-5v, but pressure points outputs 5.5v on rows 2 and 3. I want 5.5 to mean 2 octaves for this reason.
            int reduced = Proportion(cv[ch], max_cv, (12 << 7)*2); // constrain to 2 octaves (12 << 7 == 1 octave)
            return reduced;
        }
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to SwitchSeq,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
SwitchSeq SwitchSeq_instance[2];

void SwitchSeq_Start(bool hemisphere) {
    SwitchSeq_instance[hemisphere].BaseStart(hemisphere);
}

void SwitchSeq_Controller(bool hemisphere, bool forwarding) {
    SwitchSeq_instance[hemisphere].BaseController(forwarding);
}

void SwitchSeq_View(bool hemisphere) {
    SwitchSeq_instance[hemisphere].BaseView();
}

void SwitchSeq_OnButtonPress(bool hemisphere) {
    SwitchSeq_instance[hemisphere].OnButtonPress();
}

void SwitchSeq_OnEncoderMove(bool hemisphere, int direction) {
    SwitchSeq_instance[hemisphere].OnEncoderMove(direction);
}

void SwitchSeq_ToggleHelpScreen(bool hemisphere) {
    SwitchSeq_instance[hemisphere].HelpScreen();
}

uint32_t SwitchSeq_OnDataRequest(bool hemisphere) {
    return SwitchSeq_instance[hemisphere].OnDataRequest();
}

void SwitchSeq_OnDataReceive(bool hemisphere, uint32_t data) {
    SwitchSeq_instance[hemisphere].OnDataReceive(data);
}
