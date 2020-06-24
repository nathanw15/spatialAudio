#define GAMMA_H_INC_ALL         // define this to include all header files
#define GAMMA_H_NO_IO           // define this to avoid bringing AudioIO from Gamma

#include "Gamma/Gamma.h"
#include "Gamma/SamplePlayer.h"
#include "Gamma/Noise.h"
#include "Gamma/SoundFile.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Graphics.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/sound/al_Speaker.hpp"
#include "al/sphere/al_AlloSphereSpeakerLayout.hpp"
#include "al/graphics/al_Font.hpp"
#include "al/ui/al_Parameter.hpp"
#include "al/ui/al_ParameterServer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_HtmlInterfaceServer.hpp"

#include "al_ext/spatialaudio/al_Decorrelation.hpp"
//#include "al_ext/soundfile/al_SoundfileBufferedRecord.hpp"
#include "al_ext/soundfile/al_SoundfileRecordGUI.hpp"
#include "al_ext/soundfile/al_OutputRecorder.hpp"

#include <atomic>
#include <vector>

#define SAMPLE_RATE 44100
//#define BLOCK_SIZE 2048
// 1024 block size seems to fix issue where it sounded like samples were being dropped.
#define BLOCK_SIZE 1024
#define MAX_DELAY 44100
#define IR_LENGTH 2048
#define NUM_SOURCES 5

#define INPUT_CHANNELS 4

using namespace al;
using namespace std;

// 0 for 2809, 1 for Allosphere
const int location = 1;

osc::Send sender(9011, "127.0.0.1");
//ParameterServer paramServer("127.0.0.1",8080);

float radius = 5.0; //src distance

ParameterBool sampleWise("sampleWise","",1.0);
ParameterBool useDelay("useDelay","", 0.0);

Parameter masterGain("masterGain","",0.5,"",0.0,1.0);
ParameterBool soundOn("soundOn","",0.0);

SearchPaths searchpaths;

vector<string> files;
vector<string> posUpdateNames{"Static", "Trajectory", "Moving","Sine"};
vector<string> panningMethodNames{"LBAP","Source Spread","Snap to Source Width", "Snap To Nearest Speaker","Snap With Fade"};
vector<string> sourceSoundNames{"SoundFile","Noise","Sine","Impulse","Saw", "Square","AudioIn","Window Phase"};

Parameter maxDelay("maxDelay","",0.0,"",0.0,1.0);

Trigger resetSamples("resetSamples","","");
Trigger resetPosOscPhase("resetPosOscPhase","","");

ParameterMenu setAllPosUpdate("setAllPosUpdate","",0,"");
ParameterMenu setAllSoundFileIdx("setAllSoundFileIdx","",0,"");
ParameterBool setAllEnabled("setAllEnabled","",0.0);
ParameterMenu setAllPanMethod("setAllPanMethod","",0,"");
Parameter setAllAzimuth("setAllAzimuth","",2.9,"",-1.0*M_PI,M_PI);
Parameter setAllAzimuthOffsetScale("setAllAzimuthOffsetScale","",0.f,"",-1.0,1.0);
Parameter setAllElevation("setAllElevation","",0.0,"",-1.0*M_PI_2,M_PI_2);
Parameter setAllEleOffsetScale{"setAllEleOffsetScale","",0.0,"",-1.0,1.0};
ParameterMenu setAllSourceSounds("setAllSourceSounds","",0,"");

ParameterBool setAllMute("setAllMute","",0.0);
Parameter setAllGain("setAllGain","",0.0,"",0.0,1.0);
ParameterInt setAllInputChannel("setAllInputChannel","",0,"",0,INPUT_CHANNELS-1);

ParameterBool useRateMultiplier("useRateMultiplier","",0.0);
Parameter setPlayerPhase("setPlayerPhase","",0.0,"",0.0,1.0);
Parameter setAllDurations("setAllDurations","",0.0,"",0.0,10.f);

Trigger triggerAllRamps("triggerAllRamps","","");
Trigger setAllStartAzi("setAllStartAzi","","");
Trigger setAllEndAzi("setAllEndAzi","","");

Trigger setPiano("setPiano","","");
Trigger setMidiPiano("setMidiPiano","","");

ParameterBool combineAllChannels("combineAllChannels","",0.0);

ParameterMenu speakerDensity("speakerDensity","",0,"");
vector<string> spkDensityMenuNames{"All", "Skip 1", "Skip 2", "Skip 3", "Skip 4", "Skip 5"};

ParameterBool xFadeCh1_2("xFadeCh1_2","",0);
Parameter xFadeValue("xFadeValue","",0.0,"",0.0,M_PI_2);

ParameterBool setAllDecorrelate("setAllDecorrelate","",0.0);
ParameterInt sourcesToDecorrelate("sourcesToDecorrelate","",1,"",1,2);
ParameterMenu decorrelationMethod("decorrelationMethod","",0,"");
vector<string> decorMethodMenuNames{"Kendall", "Zotter"};
ParameterBool configureDecorrelation("configureDecorrelation","",0.0);

Parameter maxJump("maxJump","",M_PI,"",0.0,M_2PI);
Parameter phaseFactor("phaseFactor","",1.0,"",0.0,1.0);

Parameter deltaFreq("deltaFreq","",20.0,"", 0.0, 50.0);
Parameter maxFreqDev("maxFreqDev","",10.0,"", 0.0, 50.0);
Parameter maxTau("maxTau","",1.0,"", 0.0, 10.0);
Parameter startPhase("startPhase","",0.0,"", 0.0, 10.0);
Parameter phaseDev("phaseDev","",0.0,"", 0.0, 10.0);

Trigger generateRandDecorSeed("generateRandDecorSeed","","");

ParameterBool drawLabels("drawLabels","",1.0);

ParameterBool stereoOutput("stereoOutput","",0.0);
Parameter stereoOutputGain("stereoOutputGain","",0.2,"",0.0,1.0);
Parameter playerRateMultiplier("playerRateMultiplier","",0.002,"",0.0,0.1);

ParameterInt displaySource("displaySource","",0,"",0,NUM_SOURCES-1);

int highestChannel = 0;
int speakerCount = 0;

mutex enabledSpeakersLock;

Mat<2,double> matrix;

bool showVirtualSources = false;
bool showEvents = false;
bool showLoudspeakerGroups = false;
bool showLoudspeakers = false;
bool showRecorder = false;
bool showSetAllSources = false;
bool showXFade = false;
bool showDecorrelation = false;

struct Ramp {

    unsigned int startSample, endSample;
    bool done = false;
    bool running = false;
    bool trigger = false;
    float rampStartAzimuth = 0.0f;
    float rampEndAzimuth = 0.0f;
    float rampDuration = 0.0f;

    Ramp(){
    }

    void set(float startAzi, float endAzi, float dur){
        rampStartAzimuth = startAzi;
        rampEndAzimuth = endAzi;
        rampDuration = dur;
    }

    void start(unsigned int startSamp){
        startSample = startSamp;
        endSample = startSamp +  rampDuration *SAMPLE_RATE;
        running = true;
        done = false;
    }

    void triggerRamp(){
        trigger = true;
    }

    float next(unsigned int sampleNum){

        if(trigger){
            trigger = false;
            start(sampleNum);
        }

        if(!done && !running){
            return rampStartAzimuth;
        }else if(!done && running){

            if(sampleNum > endSample){
                sampleNum = endSample;
                done = true;
                running = false;
            }
            return (((sampleNum - startSample) * (rampEndAzimuth - rampStartAzimuth)) / (endSample - startSample)) + rampStartAzimuth;
        } else {
            return rampEndAzimuth;
        }
    }
};

class BPF {
public:
    Parameter *bpfParam;
    vector<float> bpfValues;

    float initialVal;

    float startVal;
    float targetVal;
    float currentVal;
    unsigned int startSample, endSample;
    int bpIdx = 0;

    bool running = false;
    bool triggered = false;
    bool interpolate = true;

    BPF(Parameter *param, float initVal, vector<float> bp){
        bpfParam = param;
        initialVal = startVal = initVal;
        bpfValues = bp;
    }

    void start(unsigned int startSamp){
        running = true;
        startSample = startSamp;
        bpIdx = 0;
        currentVal = startVal = initialVal;
        targetVal = bpfValues[bpIdx];
        endSample = startSamp + (bpfValues[1]*SAMPLE_RATE);
        bpfParam->set(currentVal);
    }


    void next(unsigned int sampleNum){

        if(triggered){
            triggered = false;
            start(sampleNum);
        }

        if(running){

            if(sampleNum >= endSample){ // set next breakpoint
                bpIdx +=2;

                if(bpIdx < bpfValues.size()){
                    currentVal = startVal = targetVal;
                    targetVal = bpfValues[bpIdx];
                    startSample = endSample;
                    endSample = startSample + (bpfValues[bpIdx+1]*SAMPLE_RATE);

                }else if(bpIdx >= bpfValues.size()){ // finished with breakpoints
                    currentVal = targetVal;
                    running = false;
                    bpfParam->set(currentVal);
                }

            }else{ // interpolate
                if(interpolate){
                    currentVal = (((sampleNum - startSample) * (targetVal - startVal)) / (endSample - startSample)) + startVal;
                }
                bpfParam->set(currentVal);
            }
        }
    }
};

class Event;

class TriggerEvent{
public:
    Event *eventToTrigger;
    unsigned int endSample;
    bool running = false;
    bool triggered = false;

    float triggerTime;

    TriggerEvent(Event *event, float trigTime){

        eventToTrigger = event;
        triggerTime = trigTime;
    }

    void triggerNextEvent();

    void next(unsigned int sampleNum){

        if(triggered){
            triggered = false;
            running = true;
            endSample = sampleNum + (triggerTime*SAMPLE_RATE);
        }

        if(running){
            if(sampleNum >= endSample){
                triggerNextEvent();
                running = false;
            }
        }
    }
};

class Event{
public:
    vector<BPF*> bpfs;
    vector<TriggerEvent*> triggerEvents;
    Trigger triggerEvent{"triggerEvent","",""};

    string eventName;

    vector<pair<Parameter*, float>> initialParams;
    vector<pair<ParameterBool*, float>> initialBools;
    vector<pair<ParameterMenu*, string>> initialMenus;

    Event(string name){
        eventName = name;
        triggerEvent.displayName(eventName);
        triggerEvent.registerChangeCallback([&](float val){

            for(pair<Parameter*, float> p: initialParams){
                p.first->set(p.second);
            }

            for(pair<ParameterMenu*, string> p: initialMenus){
                p.first->setCurrent(p.second);
            }

            for(BPF *e: bpfs){
                e->triggered = true;
            }

            for(TriggerEvent *tg: triggerEvents){
                tg->triggered = true;
            }
        });
    }

    void processEvent(unsigned int t){
        for(BPF *e: bpfs){
            e->next(t);
        }

        for(TriggerEvent *tg: triggerEvents){
            tg->next(t);
        }
    }

    void setParameter(Parameter *param,float val){
        initialParams.push_back(make_pair(param,val));
    }

    void setMenu(ParameterMenu *param,string val){
        initialMenus.push_back(make_pair(param,val));
    }

    void addBPF(Parameter *param, float initialVal, vector<float> breakPoints, bool interpolate = true){
        BPF *e = new BPF(param, initialVal,breakPoints);
        e->interpolate = interpolate;
        bpfs.push_back(e);
    }

    void addEvent(Event *eventToTrigger, float triggerTime){
        triggerEvents.push_back(new TriggerEvent(eventToTrigger,triggerTime));
    }

};

vector<Event*> events;

void TriggerEvent::triggerNextEvent(){
    eventToTrigger->triggerEvent.set(true);
}


void initPanner();

void wrapValues(float &val){
    while(val > M_PI){
        val -= M_2PI;
    }
    while(val < -1.f*M_PI){
        val += M_2PI;
    }
}

class WindowPhase{
public:

    float *windowBuffer;
    int windowBufferLength;

    float *audioBuffer;
    int audioBufferLength;

    ParameterInt windowLength{"windowLength","",1000,"",10,SAMPLE_RATE};
    ParameterInt windowStart{"windowStart","",0,"",0,SAMPLE_RATE};
    ParameterInt increment{"increment","",0,"",0,10000};
    ParameterBool loopWindow{"loopWindow","",0,"",0,1};

    int windowEnd = 0;
    int readPosition = 0;

    gam::SoundFile sf;

    ParameterBool printInfo{"printInfo","",0,"",0,1};


    WindowPhase(int start, int winLength, int inc, int windowBuffLen = 1024){

        windowLength.displayName("Window Length");
        windowStart.displayName("Window Start");
        increment.displayName("Increment");
        loopWindow.displayName("Loop Window");

        loadSoundfile();

        windowBufferLength = windowBuffLen;
        windowBuffer = new float[windowBufferLength];//malloc(windowBuffLen,sizeof(float));
        windowLength = winLength;

        for(int i = 0; i < windowBufferLength; i++){
            windowBuffer[i] = 0.5 * (1.0 - cos(2.0*M_PI*((float)i/((float)windowBufferLength-1.0))));
        }

        cout << windowBuffer[512] << endl;

        //setStartPos(start);

        while(start >= audioBufferLength ){
            start -= audioBufferLength;
        }
        while(start < 0){
            start += audioBufferLength;
        }
        windowStart = start;
        windowEnd = windowStart + windowLength;

        increment = inc;

        windowLength.registerChangeCallback([&](float val){
            int sEnd = windowStart.get() + val;
            wrapPosition(sEnd);
            windowEnd = sEnd;
        });

        windowStart.registerChangeCallback([&](float val){

            int sEnd = val + windowLength.get();
            wrapPosition(sEnd);
            windowEnd = sEnd;
        });

    }

    void loadSoundfile(){
        const char * path = "src/sounds/shortCount.wav";
        sf.path(path);
        bool opened = sf.openRead();
        //cout << "Opened: " << opened << endl;
        int numSamples = sf.samples();
        audioBuffer = new float[numSamples];
        audioBufferLength = numSamples;
        int read = sf.readAll(audioBuffer);
        //cout << "read: " << read << endl;
        sf.close();
        windowStart.max(numSamples);
    }

    void wrapPosition(int &value){
        if(value >= audioBufferLength){
            value -= audioBufferLength;
        }else if (value < 0){
            value += audioBufferLength;
        }
    }

    void incrementWindow(){
        int wStart = windowStart.get() + increment.get();
        wrapPosition(wStart);
        windowStart.set(wStart);
    }

    float getWindowPhase(){


        if(printInfo.get()){
            cout << "Window Start: " << windowStart.get() << " End: " << windowEnd << " inc: " << increment.get() << " bufferlen: " << audioBufferLength << endl;
                    printInfo.set(0);
        }

        if(loopWindow.get()){

            if(readPosition >= audioBufferLength){
                readPosition -= audioBufferLength;
            }

            if(windowStart.get() < windowEnd){ // ---s--------e---

                if(readPosition >= windowEnd){
                    incrementWindow();
                    readPosition = windowStart.get();
                }

                if(readPosition < windowStart.get()){
                    readPosition = windowStart.get();
                }

                int windowIdx = ((readPosition - windowStart) * (windowBufferLength)) / (windowEnd - windowStart);
                float sample = audioBuffer[readPosition];
                readPosition++;
                return sample * windowBuffer[windowIdx];


            } else { // end before start

                if(readPosition >= windowEnd && readPosition < windowStart.get()){
                    incrementWindow();
                    readPosition = windowStart.get();
                }

                if(readPosition >= windowStart.get()){ //  --e-----s--r--

                    int fakeWindowEnd = windowEnd + audioBufferLength;
                    int windowIdx = ((readPosition - windowStart) * (windowBufferLength)) / (fakeWindowEnd - windowStart);
                    float sample = audioBuffer[readPosition];
                    readPosition++;
                    return sample * windowBuffer[windowIdx];

                }else if(readPosition < windowEnd){ // --r--e-----s---

                    int fakeWindowStart = windowStart.get() - audioBufferLength;

                    int windowIdx = (((readPosition - fakeWindowStart)*(windowBufferLength))/ (windowEnd - fakeWindowStart));

                    float sample = audioBuffer[readPosition];
                    readPosition++;
                    return sample * windowBuffer[windowIdx];
                }else{
                    cout << "Should not reach here" << endl;
                }
            }


//            if(readPosition >= audioBufferLength){
//                readPosition -= audioBufferLength;
////                int wStart = windowStart.get() + increment;
////                windowEnd += increment;
////                wrapPosition(wStart);
////                windowStart.set(wStart);
////                wrapPosition(windowEnd);
//            }

//            //is greater than audiobuffer
////            if(readPosition >= audioBufferLength){
////                readPosition -= audioBufferLength;
////            }

//            //is within loop
//            if(readPosition >= windowStart.get()){
//                int readOffset = windowStart.get() + readPosition;
//                while(readOffset > audioBufferLength){
//                    readOffset -= audioBufferLength;
//                }

//                if(readOffset < windowEnd){ //within loop
//                    int windowIdx = ((readPosition - windowStart) * (windowBufferLength)) / (windowEnd - windowStart);
//                    float sample = audioBuffer[readPosition];
//                    readPosition++;
//                    return sample * windowBuffer[windowIdx];
//                }

//            }else{

//                if(windowStart.get() > windowEnd && readPosition < windowEnd){
//                    int windowIdx = ((readPosition + (audioBufferLength - windowStart)) * (windowBufferLength)) / (windowLength);
//                    float sample = audioBuffer[readPosition];
//                    readPosition++;
//                    return sample * windowBuffer[windowIdx];
//                }
//            }


////reset readPosition and
//                int wStart = windowStart.get() + increment;
//                windowEnd += increment;
//                wrapPosition(wStart);
//                windowStart.set(wStart);
//                wrapPosition(windowEnd);

//                readPosition = windowStart.get();


//            int windowIdx = ((readPosition - windowStart) * (windowBufferLength)) / (windowEnd - windowStart);
//            float sample = audioBuffer[readPosition];
//            readPosition++;
//            return sample * windowBuffer[windowIdx];





        }else {


            float gain = 0.0;

            if(readPosition >= audioBufferLength){
                readPosition -= audioBufferLength;
                //            windowStart += increment;
                int wStart = windowStart.get() + increment;
                windowEnd += increment;
                wrapPosition(wStart);
                windowStart.set(wStart);
                wrapPosition(windowEnd);
            }


            //        wrapPosition(windowStart);
            //        wrapPosition(windowEnd);


            if(readPosition >= windowStart && readPosition <= windowEnd){
                int windowIdx = ((readPosition - windowStart) * (windowBufferLength)) / (windowEnd - windowStart);
                gain = windowBuffer[windowIdx];
                //cout <<"readPos: " << readPosition << "gain: " << gain << endl;
            }else if(readPosition >= windowStart && readPosition > windowEnd){
                if(windowEnd < windowStart){// read at end with window wrapped
                    int windowIdx = ((readPosition - windowStart) * (windowBufferLength)) / (windowLength);
                    gain = windowBuffer[windowIdx];
                }
            }else if(readPosition < windowStart && readPosition < windowEnd){
                if(windowEnd < windowStart){ // read at beginning with window wrapped
                    int windowIdx = ((readPosition + (audioBufferLength - windowStart)) * (windowBufferLength)) / (windowLength);
                    gain = windowBuffer[windowIdx];
                }
            }
            float windowedSample = audioBuffer[readPosition] * gain;
            readPosition++;
            return windowedSample;
        }
    }

};


class VirtualSource {
public:

    WindowPhase wp{0,30000,10000};

    gam::SamplePlayer<> samplePlayer;
    gam::NoisePink<> noise;
    gam::Sine<> osc;
    gam::Sine<> positionOsc;
    gam::Impulse<> impulse;
    gam::Saw<> saw;
    gam::Square<> square;


    Parameter posOscPhase{"posOscPhase","", 0.0,"",0.0,1.0};
    Parameter posOscFreq{"posOscFreq","",1.0,"",0.0,5.0};
    Parameter posOscAmp{"posOscAmp","",1.0,"",0.0,M_PI};

    Parameter centerAzi{"centerAzi","",0.0,"",-1.0*M_PI,M_PI};// = 0.0;
    Parameter aziOffset{"aziOffset","",0.0,"",-1.0*M_PI,M_PI};
    Parameter aziOffsetScale{"aziOffsetScale","",0.0,"",-1.0,1.0};

    Parameter centerEle{"centerEle","",0.0,"",-1.0*M_PI_2,M_PI_2};// = 0.0;
    Parameter eleOffset{"eleOffset","",0.0,"",-1.0*M_PI,M_PI};
    Parameter eleOffsetScale{"eleOffsetScale","",0.0,"",-1.0,1.0};

    Ramp sourceRamp;

    int previousSamp = 0;
    //For "Snap with fade"
    int prevSnapChan = -1;
    int currentSnapChan = -1;
    int fadeCounter = 0;
    ParameterInt fadeDuration{"fadeDuration","",100,"",0,10000};

    ParameterBundle vsBundle{"vsBundle"};
    ParameterBool enabled{"enabled","",0.0};
    ParameterBool mute{"mute","",0.0};
    ParameterBool decorrelateSrc{"decorrelateSrc","",0};
    ParameterBool invert{"invert","",0};

    ParameterMenu positionUpdate{"positionUpdate","",0,""};
    Parameter sourceGain{"sourceGain","",0.5,"",0.0,1.0};
    Parameter aziInRad{"aziInRad","",2.9,"",-1.0*M_PI,M_PI};
    Parameter elevation{"elevation","",0.0,"",-1.0*M_PI_2,M_PI_2};
    Parameter oscFreq{"oscFreq","",440.0,"",0.0,2000.0f};
    Parameter angularFreq {"angularFreq"};//Radians per second
    Parameter angFreqCycles {"angFreqCycles", "",1.f,"",-1000.f,1000.f};
    Parameter samplePlayerRate {"samplePlayerRate","",1.f,"",1.f,1.5f};
    ParameterMenu fileMenu{"fileMenu","",0,""};

    Trigger triggerRamp{"triggerRamp","",""};
    Parameter rampStartAzimuth{"rampStartAzimuth","",-0.5,"",-1.0*M_PI,M_PI};
    Parameter rampEndAzimuth{"rampEndAzimuth","",0.5,"",-1.0*M_PI,M_PI};
    Parameter rampDuration{"rampDuration", "",1.0,"",0.0,10.0};
    ParameterMenu sourceSound{"sourceSound","",0,""};
    ParameterInt inputChannel{"inputChannel","",0,"",0,INPUT_CHANNELS-1};

    Parameter sourceWidth{"sourceWidth","", M_PI/8.0f, "", 0.0f,M_2PI};
    ParameterMenu panMethod{"panMethod","",0,""};

    ParameterBool scaleSrcWidth{"scaleSrcWidth","",0};

    Parameter soundFileStartPhase{"soundFileStartPhase","",0.0,"",0.0,1.0};
    Parameter soundFileDuration{"soundFileDuration","",1.0,"",0.0,1.0};

    Trigger loopLengthToRotFreq{"loopLengthToRotFreq","",""};
    Parameter angFreqOffset{"angFreqOffset","",0.0,"",-1.0,1.0};
    ParameterInt angFreqCyclesMult{"angFreqCyclesMult","",1,"",1,100};

    float angFreqCyclesStore = 1.0;

    VirtualSource(){

        enabled.displayName("Enabled");
        mute.displayName("Mute");
        decorrelateSrc.displayName("Decorrelate");

        sourceGain.displayName("Gain");
        panMethod.displayName("Method");
        positionUpdate.displayName("Position Update");
        sourceSound.displayName("Sound");
        fileMenu.displayName("Sound File");
        inputChannel.displayName("Audio In Ch.");

        samplePlayerRate.displayName("Rate");
        soundFileStartPhase.displayName("Start Phase");
        soundFileDuration.displayName("Duration");

        sourceWidth.displayName("Spread");
        scaleSrcWidth.displayName("Equal Loudness");

        centerAzi.displayName("Azimuth");
        aziOffset.displayName("Azi. Offset");
        aziOffsetScale.displayName("Azi. Offset Scale");

        centerEle.displayName("Elevation");
        eleOffset.displayName("Elev. Offset");
        eleOffsetScale.displayName("Elev. Offset Scale");

        angularFreq.displayName("Freq - rad/sec");
        angFreqCycles.displayName("Freq - cycles/sec");
        angFreqOffset.displayName("Freq Offset");
        angFreqCyclesMult.displayName("Freq Multiplier");

        loopLengthToRotFreq.displayName("Set Period = SoundFile Duration");

        oscFreq.displayName("Freq");

        posOscFreq.displayName("Freq");
        posOscAmp.displayName("Amp");
        posOscPhase.displayName("Phase");

        aziOffset.set( -1.0*M_PI + M_2PI * ((float)rand()) / RAND_MAX);
        eleOffset.set( -1.0*M_PI + M_2PI * ((float)rand()) / RAND_MAX);

        angularFreq.set(angFreqCycles.get()*M_2PI);
        osc.freq(oscFreq.get());
        impulse.freq(oscFreq.get());
        saw.freq(oscFreq.get());
        square.freq(oscFreq.get());

        positionOsc.freq(posOscFreq.get());

        panMethod.setElements(panningMethodNames);
        sourceSound.setElements(sourceSoundNames);
        positionUpdate.setElements(posUpdateNames);
        fileMenu.setElements(files);
        samplePlayer.load("src/sounds/count.wav");

        fileMenu.setCurrent("count.wav");
        samplePlayer.rate(1.0);
        samplePlayerRate.set(1.0);
        soundFileDuration.max(samplePlayer.period());

        sourceRamp.rampStartAzimuth = rampStartAzimuth.get();
        sourceRamp.rampEndAzimuth = rampEndAzimuth.get();
        sourceRamp.rampDuration = rampDuration.get();

        angFreqOffset.registerChangeCallback([&](float val){
           angFreqCycles.set(angFreqCyclesStore + val);
        });

        loopLengthToRotFreq.registerChangeCallback([&](float val){
            angFreqCyclesStore = (float) angFreqCyclesMult * (1.0/(float)samplePlayer.period());
            angFreqCycles.set(angFreqCyclesStore + angFreqOffset.get());
        });


        angFreqCyclesMult.registerChangeCallback([&](float val){
            angFreqCyclesStore = val * (1.0/(float)samplePlayer.period());
            angFreqCycles.set(angFreqCyclesStore + angFreqOffset.get());
        });

        soundFileStartPhase.registerChangeCallback([&](float val){
            samplePlayer.range(val,soundFileDuration);
        });

        soundFileDuration.registerChangeCallback([&](float val){
            samplePlayer.range(soundFileStartPhase,val);
        });

        centerAzi.registerChangeCallback([&](float val){
            val += aziOffset*aziOffsetScale;
            wrapValues(val);
           aziInRad = val;
        });

        aziOffset.registerChangeCallback([&](float val){
            val *= aziOffsetScale;
            val += centerAzi;
            wrapValues(val);
           aziInRad = val;
        });

        aziOffsetScale.registerChangeCallback([&](float val){
           val *= aziOffset;
           val += centerAzi;
           wrapValues(val);
          aziInRad = val;
        });

        centerEle.registerChangeCallback([&](float val){
            val += eleOffset*eleOffsetScale;

           elevation = val;
        });

        eleOffset.registerChangeCallback([&](float val){
            val *= eleOffsetScale;
            val += centerEle;
            elevation = val;
        });

        eleOffsetScale.registerChangeCallback([&](float val){
           val *= eleOffset;
           val += centerEle;
           elevation = val;
        });

        posOscPhase.registerChangeCallback([&](float val){
            positionOsc.phase(val);
         });

        posOscFreq.registerChangeCallback([&](float val){
           positionOsc.freq(val);
        });

        angFreqCycles.registerChangeCallback([&](float val){
            angularFreq.set(val*M_2PI);
        });

        oscFreq.registerChangeCallback([&](float val){
           osc.freq(val);
           impulse.freq(val);
           saw.freq(val);
           square.freq(val);
        });

        aziInRad.setProcessingCallback([&](float val){
            wrapValues(val);
            return val;
        });

        samplePlayerRate.registerChangeCallback([&](float val){
           samplePlayer.rate(val);
        });

        fileMenu.registerChangeCallback([&](float val){
            samplePlayer.load(searchpaths.find(files[val]).filepath().c_str());
            soundFileDuration.max(samplePlayer.period());
        });

        triggerRamp.registerChangeCallback([&](float val){
            sourceRamp.triggerRamp();
            samplePlayer.reset();
        });

        rampStartAzimuth.registerChangeCallback([&](float val){
                sourceRamp.rampStartAzimuth = val;
        });

        rampEndAzimuth.registerChangeCallback([&](float val){
                sourceRamp.rampEndAzimuth = val;
        });

        rampDuration.registerChangeCallback([&](float val){
                sourceRamp.rampDuration = val;
        });

        vsBundle << enabled << mute << decorrelateSrc << sourceGain << panMethod << positionUpdate << sourceSound <<  fileMenu  << samplePlayerRate << soundFileStartPhase << soundFileDuration << centerAzi << aziOffset << aziOffsetScale << centerEle << eleOffset << eleOffsetScale << angularFreq << angFreqCycles << angFreqOffset << angFreqCyclesMult << loopLengthToRotFreq << oscFreq  << scaleSrcWidth << sourceWidth << fadeDuration << posOscFreq << posOscAmp << posOscPhase;

    }


    void updatePosition(unsigned int sampleNumber){

        switch ((int)positionUpdate.get()) {
        case 0:
            break;
        case 1:
            aziInRad.set(sourceRamp.next(sampleNumber));
            break;
        case 2: {
            float aziDelta = angularFreq.get()*(sampleNumber - previousSamp)/SAMPLE_RATE;
            aziInRad.set(aziInRad.get()+aziDelta);
            previousSamp = sampleNumber;
            break;
        }
        case 3:{
            float temp = centerAzi + (aziOffset* aziOffsetScale) + (positionOsc() * posOscAmp.get());
            wrapValues(temp); //wrapValues is also called by aziInRad callback...
            aziInRad.set(temp);
        }
        default:
            break;
        }
    }

    float getSample(AudioIOData &io, int &channel){

        float sample;

        switch ((int)sourceSound.get() ) {
        case 0:
            if(samplePlayer.done()){
                samplePlayer.reset();
            }
            sample = samplePlayer();
            break;

        case 1:
            sample = noise();
            break;
        case 2:
            sample = osc() * 0.2;
            break;
        case 3:
            sample = impulse() * 0.2;
            break;
        case 4:
            sample = saw() * 0.2;
            break;
        case 5:
            sample = square() * 0.2;
            break;
        case 6:
            sample = io.in(channel,io.frame());
            break;
        case 7:
            sample = wp.getWindowPhase();
            break;
        default:
            sample = 0.0;
            break;
        }

        if(invert.get()){
            sample *= -1.0;
        }

        if(mute.get()){
            sample = 0.0;
        }

        return sample * sourceGain.get();

    }

    float getSamplePlayerPhase(){
        return samplePlayer.pos()/samplePlayer.frames();
    }

    void getFadeGains(float &prevGain, float &currentGain){
        float samplesToFade = fadeDuration.get();
        if(fadeCounter < samplesToFade){
            float val = (fadeCounter/samplesToFade) * M_PI_2;
            currentGain = sin(val);
            prevGain = cos(val);
            fadeCounter++;
        }else{
            currentGain = 1.0;
            prevGain = 0.0;
        }
    }
};

vector<VirtualSource*> sources;

class SpeakerV: public Speaker {
public:

    ParameterBool *enabled;
    Parameter *speakerGain;
    std::string oscTag;
    std::string deviceChannelString;
    float aziInRad;

    int delay;
    void *buffer;
    int bufferSize;
    int readPos,writePos;

    bool isPhantom = false;

    int decorrelationOutputIdx = 0;

    SpeakerV(int chan, float az=0.f, float el=0.f, int gr=0, float rad=1.f, float ga=1.f, int del = 0){
        delay = del;
        readPos = 0;
        writePos = 0;

        setDelay(maxDelay.get()*SAMPLE_RATE);
        bufferSize = 44100*2;//MAKE WORK WITH MAX DELAY
        buffer = calloc(bufferSize,sizeof(float));
        deviceChannel = chan;
        azimuth= az;
        elevation = el;
        group = gr;
        radius = rad;
        gain = ga;
        aziInRad = toRad(az);
        deviceChannelString = std::to_string(deviceChannel);
        oscTag = "speaker"+ deviceChannelString + "/enabled";

        enabled = new ParameterBool(oscTag,"",1.0);
        enabled->registerChangeCallback([&](bool b){
            initPanner(); //TODO: enabled is set after this. initPanner Should be called after this callback not in it.
        });

        string gainTag = "speaker" + deviceChannelString + "/speakerGain";
        speakerGain = new Parameter(gainTag,"",1.0,"",0.0,1.0);
        //paramServer.registerParameter(*enabled);
    }

    void setDelay(int delayInSamps){
        delay = rand() % static_cast<int>(delayInSamps + 1);
    }

    float read(){
        if(readPos >= bufferSize){
            readPos = 0;
        }
        float *b = (float*)buffer;
        float val = b[readPos];
        readPos++;
        return val;
    }

    void write(float samp){
        if(writePos >= bufferSize){
            writePos -= bufferSize;
        }

        int writeDelay = writePos + delay;
        if(writeDelay >= bufferSize){
            writeDelay -= bufferSize;
        }

        float *b = (float*)buffer;
        b[writeDelay] = samp;
        writePos++;
    }
};


class SpeakerLayer {
public:
    std::vector<SpeakerV> l_speakers;
    std::vector<SpeakerV*> l_enabledSpeakers;
    float elevation;

    SpeakerLayer(){

    }
};

std::vector<SpeakerLayer> layers;
std::vector<SpeakerV *> allSpeakers;


bool speakerSort(SpeakerV const *first, SpeakerV const *second){
    return first->azimuth < second->azimuth;
}

void initPanner(){

    enabledSpeakersLock.lock();
    for(SpeakerLayer &sl: layers){
        sl.l_enabledSpeakers.clear();
        for(int i = 0; i < sl.l_speakers.size(); i ++){
            if(sl.l_speakers[i].enabled->get() > 0.5){
                sl.l_enabledSpeakers.push_back(&sl.l_speakers[i]);
            }
        }
        std::sort(sl.l_enabledSpeakers.begin(),sl.l_enabledSpeakers.end(),&speakerSort);
    }
    enabledSpeakersLock.unlock();
}


class SpeakerGroup{
public:
    vector<SpeakerV*> speakers;
    string groupName = "";
    ParameterBool enable{"enable","",1,"",0,1};
    Parameter gain{"groupGain","",0.0,"",0.0,1.0};

    SpeakerGroup(string name){
        groupName = name;
        enable.displayName("Enable");
        gain.displayName("Gain");

        enable.registerChangeCallback([&](float val){
            for(SpeakerV *s: speakers){
                if(val == 1.0){
                    if(s->enabled->get() == 0.0){
                        s->enabled->set(1.0);
                    }
                }else{
                    if(s->enabled->get() == 1.0){
                        s->enabled->set(0.0);
                    }
                }
            }
        });

        gain.registerChangeCallback([&](float val){
           for(SpeakerV *s: speakers){
               s->speakerGain->set(val);
           }
        });

    }

    void addSpeaker(SpeakerV *speaker){
        speakers.push_back(speaker);
    }

    void addSpeakersByChannel(vector<int> channels){
        for(int ch: channels){
            for(SpeakerV *s: allSpeakers){
                if(s->deviceChannel == ch){
                    addSpeaker(s);
                    break;
                }
            }
        }
    }

};

vector<SpeakerGroup *> speakerGroups;

struct MyApp : public App
{
public:
    Mesh mSpeakerMesh;
    vector<Mesh> mVec;
    vector<int>  sChannels;
    atomic<float> *mPeaks {nullptr};
    float speedMult = 0.03f;
    Vec3d srcpos {0.0,0.0,0.0};
    ControlGUI parameterGUI;
    //ParameterBundle xsetAllBundle{"xsetAllBundle"};

    //Size of the decorrelation filter. See Kendall p. 75
    //How does this translate to the duration of the impulse response?
    //Decorrelation decorrelation{BLOCK_SIZE*2}; //Check to see if this is correct
    Decorrelation decorrelation{IR_LENGTH};
    int decorrelationSeed = 1000;

    map<uint32_t, vector<uint32_t>> routingMap;

    Font font;
    Mesh fontMesh;

    std::string pathToInterfaceJs = "interface.js";
    HtmlInterfaceServer htmlServer{pathToInterfaceJs};

    OutputRecorder soundFileRecorder;

    MyApp()
    {

        searchpaths.addAppPaths();
        searchpaths.addRelativePath("src/sounds");

        FileList fl = itemListInDir("src/sounds");
        sender.send("/files","clear");
        for(int i = 0; i < fl.count(); i++){
            string fPath = fl[i].filepath();
            fPath = fPath.substr(fPath.find_last_of("/\\")+1);
            files.push_back(fPath);
            sender.send("/files",fPath);
        }

        parameterGUI << soundOn << masterGain << stereoOutput << resetPosOscPhase << sampleWise << combineAllChannels << xFadeCh1_2 << xFadeValue << sourcesToDecorrelate << decorrelationMethod << generateRandDecorSeed << maxJump << phaseFactor << deltaFreq << maxFreqDev << maxTau << startPhase << phaseDev;

        //xsetAllBundle << setAllEnabled << setAllDecorrelate << setAllPanMethod << setAllPosUpdate << setAllSoundFileIdx <<setAllAzimuth << setAllAzimuthOffsetScale << playerRateMultiplier << useRateMultiplier << setPlayerPhase << triggerAllRamps << setAllStartAzi << setAllEndAzi << setAllDurations;
        //parameterGUI << xsetAllBundle;

        for(int i = 0; i < NUM_SOURCES; i++){
            auto *newVS = new VirtualSource; // This memory is not freed and it should be...
            sources.push_back(newVS);
            //parameterGUI << newVS->vsBundle;
            parameterServer() << newVS->vsBundle;
            //parameterServer() << newVS->sourceGain << newVS->centerAzi << newVS->centerEle;
        }

//        parameterServer() << soundOn << resetPosOscPhase
//                          <<  masterGain << setAllDecorrelate
//                          << decorrelationMethod << speakerDensity << drawLabels
//                          << xFadeCh1_2 << xFadeValue << generateRandDecorSeed
//                          << maxJump << phaseFactor << deltaFreq << maxFreqDev
//                          << maxTau << startPhase << phaseDev;

        //example with set all
        parameterServer() << setAllGain << setPlayerPhase << playerRateMultiplier
                          << setAllAzimuth << setAllAzimuthOffsetScale << setAllElevation
                          << setAllEleOffsetScale;

        htmlServer << parameterServer();

        soundOn.displayName("Sound On");
        masterGain.displayName("Master Gain");
        stereoOutput.displayName("Stereo Output");
        stereoOutputGain.displayName("Stereo Output Gain");
        xFadeCh1_2.displayName("Enabled");
        xFadeValue.displayName("value");

        setAllEnabled.displayName("Enabled");
        setAllDecorrelate.displayName("Decorrelate");
        setAllPanMethod.displayName("Method");
        setAllPosUpdate.displayName("Position Update");
        setAllSoundFileIdx.displayName("Sound File");
        setAllSourceSounds.displayName("Sound");

        setAllMute.displayName("Mute");
        setAllGain.displayName("Gain");
        setAllInputChannel.displayName("Audio In Ch.");


        setPlayerPhase.displayName("Phase");
        playerRateMultiplier.displayName("Rate Multiplier");
        useRateMultiplier.displayName("Use Rate Multiplier");

        setAllAzimuth.displayName("Azimuth");
        setAllAzimuthOffsetScale.displayName("Azi. Offset Scale");
        setAllElevation.displayName("Elevation");
        setAllEleOffsetScale.displayName("Elev. Offset Scale");
        resetPosOscPhase.displayName("Reset Position Osc. Phase");

        decorrelationMethod.displayName("Method");
        generateRandDecorSeed.displayName("New Random Seed");
        maxJump.displayName("Max Jump");
        phaseFactor.displayName("Phase Factor");

        deltaFreq.displayName("Delta Freq.");
        maxFreqDev.displayName("Max Freq. Deviation");
        maxTau.displayName("Max Tau");
        startPhase.displayName("Start Phase");
        phaseDev.displayName("Phase Deviation");

        displaySource.displayName("Source Index");

        sampleWise.setHint("hide", 1.0);
        combineAllChannels.setHint("hide", 1.0);
        sourcesToDecorrelate.setHint("hide", 1.0);
        triggerAllRamps.setHint("hide", 1.0);
        setAllStartAzi.setHint("hide", 1.0);
        setAllEndAzi.setHint("hide", 1.0);
        setAllDurations.setHint("hide", 1.0);


        setAllPanMethod.setElements(panningMethodNames);
        setAllPosUpdate.setElements(posUpdateNames);
        setAllSoundFileIdx.setElements(files);
        setAllSourceSounds.setElements(sourceSoundNames);
        speakerDensity.setElements(spkDensityMenuNames);
        decorrelationMethod.setElements(decorMethodMenuNames);

        setAllMute.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->mute.set(val);
            }
        });

        setAllGain.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->sourceGain.set(val);
            }
        });

        setAllInputChannel.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->inputChannel.set(val);
            }
        });

        setAllSourceSounds.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->sourceSound.set(val);
            }
        });

        decorrelationMethod.registerChangeCallback([&](float val){

            if(val == 0){ //Kendall
                maxJump.setHint("hide", 0.0);
                phaseFactor.setHint("hide", 0.0);
                deltaFreq.setHint("hide", 1.0);
                maxFreqDev.setHint("hide", 1.0);
                maxTau.setHint("hide", 1.0);
                startPhase.setHint("hide", 1.0);
                phaseDev.setHint("hide", 1.0);
            }else if(val == 1){
                maxJump.setHint("hide", 1.0);
                phaseFactor.setHint("hide", 1.0);
                deltaFreq.setHint("hide", 0.0);
                maxFreqDev.setHint("hide", 0.0);
                maxTau.setHint("hide", 0.0);
                startPhase.setHint("hide", 0.0);
                phaseDev.setHint("hide", 0.0);
            }

            configureDecorrelation.set(1.0);
        });

        decorrelationMethod.set(0);

        generateRandDecorSeed.registerChangeCallback([&](float val){
            decorrelationSeed = rand() % static_cast<int>(1001);
            cout << "Decorrelation Seed: " << decorrelationSeed << endl;
            configureDecorrelation.set(1.0);
        });

        maxJump.registerChangeCallback([&](float val){configureDecorrelation.set(1.0);});
        phaseFactor.registerChangeCallback([&](float val){configureDecorrelation.set(1.0);});
        deltaFreq.registerChangeCallback([&](float val){configureDecorrelation.set(1.0);});
        maxFreqDev.registerChangeCallback([&](float val){configureDecorrelation.set(1.0);});
        maxTau.registerChangeCallback([&](float val){configureDecorrelation.set(1.0);});
        startPhase.registerChangeCallback([&](float val){configureDecorrelation.set(1.0);});
        phaseDev.registerChangeCallback([&](float val){configureDecorrelation.set(1.0);});

        resetPosOscPhase.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->posOscPhase.set(0.0);
            }
        });

        setPlayerPhase.registerChangeCallback([&](float val){

            for(VirtualSource *v: sources){
                v->samplePlayer.reset();
                int idx = v->vsBundle.bundleIndex();
                int newPos = val*v->samplePlayer.frames()*idx;
                while(newPos >= v->samplePlayer.frames()){
                    newPos -= v->samplePlayer.frames();
                }
                v->samplePlayer.pos(newPos);
            }
        });

        setAllDecorrelate.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->decorrelateSrc.set(val);
            }
        });

        useRateMultiplier.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                if(val == 1.f){
                    float newRate = 1.0+ (v->vsBundle.bundleIndex()*playerRateMultiplier.get());
                    v->samplePlayerRate.set(newRate);
                }else{
                    v->samplePlayerRate.set(1.0);
                }
            }
        });

        playerRateMultiplier.registerChangeCallback([&](float val){
            if(useRateMultiplier.get()){
                for(VirtualSource *v: sources){
                    float newRate = 1.0+ (v->vsBundle.bundleIndex()*val);
                    v->samplePlayerRate.set(newRate);
                }
            }
        });

        setAllAzimuthOffsetScale.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->aziOffsetScale.set(val);
            }
        });

        setAllAzimuth.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                float newAzi = val;
                wrapValues(newAzi);
                v->aziInRad.set(newAzi);
                v->centerAzi.set(newAzi);
            }
        });

        setAllElevation.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->elevation.set(val);
                v->centerEle.set(val);
            }
        });

        setAllEleOffsetScale.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->eleOffsetScale.set(val);
            }
        });

        setAllEnabled.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->enabled.set(val);
            }
        });

        setAllPanMethod.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->panMethod.set(val);
            }
        });
        
        setPiano.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                switch (v->vsBundle.bundleIndex()) {
                    case 0:
                        v->samplePlayer.load("src/sounds/pianoA.wav");
                        v->enabled.set(1.f);
                        break;
                    case 1:
                        v->samplePlayer.load("src/sounds/pianoB.wav");
                        v->enabled.set(1.f);
                        break;
                    case 2:
                        v->samplePlayer.load("src/sounds/pianoC.wav");
                        v->enabled.set(1.f);
                        break;
                    case 3:
                        v->samplePlayer.load("src/sounds/pianoD.wav");
                        v->enabled.set(1.f);
                        break;
                    default:
                        v->enabled.set(0.f);
                        break;
                }
            }
        });

         setMidiPiano.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                switch (v->vsBundle.bundleIndex()) {
                    case 0:
                        v->samplePlayer.load("src/sounds/midiPiano.wav");
                        v->enabled.set(1.f);
                        break;
                    case 1:
                        v->samplePlayer.load("src/sounds/midiPiano.wav");
                        v->enabled.set(1.f);
                        break;
                    case 2:
                        v->samplePlayer.load("src/sounds/midiPiano.wav");
                        v->enabled.set(1.f);
                        break;
                    case 3:
                        v->samplePlayer.load("src/sounds/midiPiano.wav");
                        v->enabled.set(1.f);
                        break;
                    default:
                        v->enabled.set(0.f);
                        break;
                }
            }
        });

        setAllPosUpdate.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->positionUpdate.set(val);
            }
        });

        setAllSoundFileIdx.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->fileMenu.set(val);
            }
        });

        triggerAllRamps.registerChangeCallback([&](float val){
            if(val == 1.f){
                for(VirtualSource *v: sources){
                    v->triggerRamp.set(1.f);
                }
            }
        });

        setAllStartAzi.registerChangeCallback([&](float val){
            if(val == 1.f){
                for(VirtualSource *v: sources){
                    v->rampStartAzimuth.set(v->aziInRad);
                }
            }
        });

        setAllEndAzi.registerChangeCallback([&](float val){
            if(val == 1.f){
                for(VirtualSource *v: sources){
                    v->rampEndAzimuth.set(v->aziInRad);
                }
            }
        });

        setAllDurations.registerChangeCallback([&](float val){

            for(VirtualSource *v: sources){
                v->rampDuration.set(val);
            }
        });

//        maxDelay.registerChangeCallback([&](float val){
//            int delSamps = val*SAMPLE_RATE;
//            for(SpeakerV &v:speakers){
//               v.setDelay(delSamps);
//            }
//        });

        resetSamples.registerChangeCallback([&](float val){
            for(VirtualSource *v: sources){
                v->samplePlayer.reset();
            }
        });

        speakerDensity.registerChangeCallback([&](float val){
            for(SpeakerLayer& sl: layers){
                for(int i = 0; i < sl.l_speakers.size(); i++){
                    SpeakerV v = sl.l_speakers[i];
                    if(v.deviceChannel != -1){
                        int tempVal = v.deviceChannel % ((int)val+1);
                        if(tempVal == 0){
                            v.enabled->set(1.0f);
                        }else {
                            v.enabled->set(0.0f);
                        }
                    }
                }
            }
        });

        parameterServer().sendAllParameters("127.0.0.1", 9011);
    }

    void createMatrix(Vec3d left, Vec3d right){
        matrix.set(left.x,left.y,right.x,right.y);
    }

    Vec3d ambiSphericalToOGLCart(float azimuth, float elevation, float radius){
        Vec3d ambiSpherical;

        //find xyz in cart audio coords
        float x = radius * cos(elevation) * cos(azimuth);
        float y = radius * cos(elevation) * sin(azimuth);
        float z = radius * sin(elevation);

        //convert to open_gl cart coords
        ambiSpherical[0] = -y;
        ambiSpherical[1] = z;
        ambiSpherical[2] = -x;

        return ambiSpherical;
    }

    void openGLCartToAmbiCart(Vec3f &vec){
        Vec3f tempVec = vec;
        vec[0] = tempVec.z*-1.0;
        vec[1] = tempVec.x*-1.0;
        vec[2] = tempVec.y;
    }

    //TODO: io not used here
    Vec3d calcGains(SpeakerLayer &sl, const float &srcAzi, SpeakerV* &speakerChan1, SpeakerV* &speakerChan2){

        Vec3f ambiCartSrcPos = ambiSphericalToOGLCart(srcAzi,0.0,radius); //Disregard elevation
        openGLCartToAmbiCart(ambiCartSrcPos);
        //std::sort(enabledSpeakers.begin(),enabledSpeakers.end(),&speakerSort);
        Vec3d gains(0.,0.,0.);
        //Vec3d gains;
        //float speakSrcAngle,linearDistance;

        //if(enabledSpeakersLock.try_lock()){ //TODO: handle differently. enabled speakers is cleared by initPanner() and needs to be locked when updating
        if(true){

        //check if source is beyond the first or last speaker
//            if(srcAzi < enabledSpeakers[0]->aziInRad){
//                speakerChan1 = enabledSpeakers[0]->deviceChannel;
//                speakerChan2 = enabledSpeakers[0+1]->deviceChannel;
//                speakSrcAngle = fabsf(enabledSpeakers[0]->aziInRad - srcAzi);
//                gains.x = 1.f / (radius * (M_PI - speakSrcAngle));

//            } else if(srcAzi > enabledSpeakers[enabledSpeakers.size()-1]->aziInRad){
//                speakerChan1 = enabledSpeakers[enabledSpeakers.size()-2]->deviceChannel;//set to speaker before last
//                speakerChan2 = enabledSpeakers[enabledSpeakers.size()-1]->deviceChannel;
//                speakSrcAngle = fabsf(enabledSpeakers[enabledSpeakers.size()-1]->aziInRad - srcAzi);
//                linearDistance = 2.0*radius*cos((M_PI - speakSrcAngle)/2.0);
//                gains.y = 1.f / (radius * (M_PI - speakSrcAngle));

//            }

//            else{//Source between first and last speakers
//                for(int i = 0; i < enabledSpeakers.size()-1; i++){
//                    speakerChan1 = enabledSpeakers[i]->deviceChannel;
//                    speakerChan2 = enabledSpeakers[i+1]->deviceChannel;
//                    if(srcAzi == enabledSpeakers[i]->aziInRad ){
//                        gains.x = 1.0;
//                        break;
//                    }else if(srcAzi > enabledSpeakers[i]->aziInRad && srcAzi < enabledSpeakers[i+1]->aziInRad){
//                        createMatrix(enabledSpeakers[i]->vec(),enabledSpeakers[i+1]->vec());
//                        invert(matrix);
//                        for (unsigned i = 0; i < 2; i++){
//                            for (unsigned j = 0; j < 2; j++){
//                                gains[i] += ambiCartSrcPos[j] * matrix(j,i);
//                            }
//                        }
//                        gains = gains.normalize();
//                        break;
//                    } else if(srcAzi == enabledSpeakers[i+1]->aziInRad){
//                        gains.y = 1.0;
//                        break;
//                    }
//                }
//            }

            //cout << sl.l_enabledSpeakers.size() << endl;
            for(int i = 0; i < sl.l_enabledSpeakers.size(); i++){

//                speakerChan1 = sl.l_enabledSpeakers[i]->deviceChannel;
                speakerChan1 = sl.l_enabledSpeakers[i];
                //speakerChan1 = layers[0].l_enabledSpeakers[0];

                int chan2Idx;

                if(i+1 == sl.l_enabledSpeakers.size()){
                    chan2Idx = 0;
                }else{
                    chan2Idx = i+1;
                }

//                speakerChan2 = sl.l_enabledSpeakers[chan2Idx]->deviceChannel;
                speakerChan2 = sl.l_enabledSpeakers[chan2Idx];

                if(srcAzi == sl.l_enabledSpeakers[i]->aziInRad ){
                    gains.x = 1.0;
                    break;
                }

                else if(srcAzi == sl.l_enabledSpeakers[chan2Idx]->aziInRad){
                    gains.y = 1.0;
                    break;
                }

 //               else if(srcAzi > enabledSpeakers[i]->aziInRad && srcAzi < enabledSpeakers[chan2Idx]->aziInRad){
                    createMatrix(sl.l_enabledSpeakers[i]->vec(),sl.l_enabledSpeakers[chan2Idx]->vec());
                    invert(matrix);
                    for (unsigned i = 0; i < 2; i++){
                        for (unsigned j = 0; j < 2; j++){
                            gains[i] += ambiCartSrcPos[j] * matrix(j,i);
                        }
                    }

                    if((gains[0] >= 0) && (gains[1] >=0)){

                        gains = gains.normalize();
                        break;
                    } else{
                        gains.x = gains.y = 0.0;
                    }
//                }


            }

           //enabledSpeakersLock.unlock();

        }else{
//            speakerChan1 = speakerChan2 = -1;
            speakerChan1 = speakerChan2 = nullptr;
        }
        return gains;
    }

    //Returns the magnitude of the angle difference from 0 - Pi
    float getAbsAngleDiff(float const &angle1, float const &angle2){
        float diff = angle1 - angle2;
        diff += (diff > M_PI) ? -1.0f*M_2PI : (diff < -1.0f*M_PI) ? M_2PI : 0.0f;
        return fabs(diff);
    }


    float calcSpeakerSkirtGains(float srcAzi, float srcElev, float spkSkirtWidth, float spkAzi, float spkElev){
        float gain = 0.0f;
        float distanceToSpeaker = getAbsAngleDiff(srcAzi, spkAzi) + getAbsAngleDiff(srcElev, spkElev);
        if(distanceToSpeaker <= spkSkirtWidth/2.0f){
            float p = ((2.0f*distanceToSpeaker)/(spkSkirtWidth/2.0f))-1.0f;
            gain = cos((M_PI*(p+1))/4.0f);
            return gain;
        }else{
            return 0.0f;
        }
    }

    void setOutput(AudioIOData &io, int channel, int frame, float sample){
        if(channel != -1){
            io.out(channel,frame) += sample*masterGain.get();
        }
    }

    void BufferDelaySpeakers(AudioIOData &io,const float &srcAzi, const float *buffer){
//        int speakerChan1, speakerChan2;
//        Vec3d gains = calcGains(io,srcAzi, speakerChan1, speakerChan2);

//        for(int i = 0; i < enabledSpeakers.size(); i++){
//            SpeakerV *s = enabledSpeakers[i];
//            for(int j = 0; j < io.framesPerBuffer();j++){
//                if(s->deviceChannel == speakerChan1){
//                    s->write(buffer[j]*gains[0]);
//                }else if(s->deviceChannel == speakerChan2){
//                    s->write(buffer[j]*gains[1]);
//                }else{
//                    s->write(0.0);
//                }
//                setOutput(io,s->deviceChannel,j,s->read());
//            }
//        }
    }

    void renderSampleDelaySpeakers(AudioIOData &io,const float &srcAzi, const float &sample){
//        int speakerChan1, speakerChan2;
//        Vec3d gains = calcGains(io,srcAzi, speakerChan1, speakerChan2);

//        for(int i = 0; i < enabledSpeakers.size(); i++){
//            SpeakerV *s = enabledSpeakers[i];
//            if(s->deviceChannel == speakerChan1){
//                s->write(sample*gains[0]);
//            }else if(s->deviceChannel == speakerChan2){
//                s->write(sample*gains[1]);
//            }else{
//                s->write(0.0);
//            }
//            setOutput(io,s->deviceChannel,io.frame(),s->read());
//        }
    }

    void renderBuffer(AudioIOData &io,const float &srcAzi, const float *buffer){
//        int speakerChan1, speakerChan2;
//        Vec3d gains = calcGains(io,srcAzi, speakerChan1, speakerChan2);
//        for(int i = 0; i < io.framesPerBuffer(); i++){
//            setOutput(io,speakerChan1,i,buffer[i]*gains[0]);
//            setOutput(io,speakerChan2,i,buffer[i]*gains[1]);
//        }
    }

      void renderSample(AudioIOData &io, VirtualSource  *vs){

        int vsIndex = vs->vsBundle.bundleIndex();
        int outputBufferOffset = (highestChannel+1)*vsIndex;

        int inputChannel = vs->inputChannel.get();

        float xFadeGain = 1.0;

        if(xFadeCh1_2.get() && vsIndex < 2 ){
            if(vsIndex == 0){
                xFadeGain = cos(xFadeValue.get());
            }else if(vsIndex == 1){
                xFadeGain = sin(xFadeValue.get());
            }
        }

        if(vs->panMethod.get() == 0){ // VBAP

            std::vector<float> layerGains(layers.size(),0.0);

            if(layers.size() > 1){
                if(vs->elevation < layers[0].elevation){
                    //Source is below bottom ring
                    layerGains[0] = 1.0;
                }else{
                    for(int i = 0; i < layers.size(); i++){

                        if(vs->elevation == layers[i].elevation){
                            layerGains[i] = 1.0;
                            break;
                        }else if(i == layers.size()-1){
                            if( layers[i].elevation < vs->elevation){
                                //Source is over top ring
                                layerGains[layers.size()-1] = 1.0;
                                break;
                            }
                        } else if((layers[i].elevation < vs->elevation) && (vs->elevation < layers[i+1].elevation)){
                            //Source Between two layers
                            float fraction = (vs->elevation - layers[i].elevation) /
                                             (layers[i+1].elevation - layers[i].elevation);  // elevation angle between layers
                            layerGains[i] = cos(M_PI_2 * fraction);
                            layerGains[i+1] = sin(M_PI_2 * fraction);
                            break;

                        }
                    }
                }
            }else{
                //Only one layer so make its gain 1
                layerGains[0] = 1.0;
            }

            float sample;
            if(!vs->decorrelateSrc.get()){
                sample = vs->getSample(io,inputChannel);
            }

            for(int i = 0; i < layers.size(); i++){

                if(layerGains[i] > 0.0){
                    SpeakerV *speaker1;
                    SpeakerV *speaker2;

                    Vec3d gains = calcGains(layers[i],vs->aziInRad, speaker1, speaker2);

                    gains[0] *= layerGains[i];
                    gains[1] *= layerGains[i];
                    float sampleOut1, sampleOut2;

                    if(vs->decorrelateSrc.get()){
                        if(speaker1 != nullptr){
                            sampleOut1 = decorrelation.getOutputBuffer(speaker1->decorrelationOutputIdx)[io.frame()];
                            setOutput(io,speaker1->deviceChannel,io.frame(),sampleOut1 * gains[0] * xFadeGain * speaker1->speakerGain->get());
                        }
                        if(speaker2 != nullptr){
                            sampleOut2 = decorrelation.getOutputBuffer(speaker2->decorrelationOutputIdx)[io.frame()];
                            setOutput(io,speaker2->deviceChannel,io.frame(),sampleOut2 * gains[1] * xFadeGain * speaker2->speakerGain->get());
                        }
                    } else{ // don't decorrelate
                        setOutput(io,speaker1->deviceChannel,io.frame(),sample * gains[0] * xFadeGain * speaker1->speakerGain->get());
                        setOutput(io,speaker2->deviceChannel,io.frame(),sample * gains[1] * xFadeGain * speaker2->speakerGain->get());
                    }
                }
            }

        }else if(vs->panMethod.get()==1 || vs->panMethod.get() == 2){ //Skirt and snap source width

            std::vector<float> gains(highestChannel+1,0.0);
            float gainsAccum = 0.0;
            float sample = 0.0f;
            if(!vs->decorrelateSrc.get()){
                sample = vs->getSample(io, inputChannel);
            }

            for(SpeakerLayer& sl: layers){
                for (int i = 0; i < sl.l_speakers.size(); i++){
                    if(!sl.l_speakers[i].isPhantom && sl.l_speakers[i].enabled->get()){
                        int speakerChannel = sl.l_speakers[i].deviceChannel;
                        float gain = calcSpeakerSkirtGains(vs->aziInRad, vs->elevation,vs->sourceWidth,sl.l_speakers[i].aziInRad, sl.elevation);

                        if(vs->panMethod.get() == 2 && gain > 0.0){
                            gain = 1.0;
                        }

                        //TODO: speakerGain not used here
                        gains[speakerChannel] = gain;
                        gainsAccum += gain*gain;
                    }
                }
            }

            float gainScaleFactor = 1.0;
            if(vs->scaleSrcWidth.get()){
                if(!gainsAccum == 0.0){
                    gainScaleFactor = sqrt(gainsAccum);
                }
            }

            for(SpeakerLayer& sl: layers){
                for (int i = 0; i < sl.l_speakers.size(); i++){
                    if(!sl.l_speakers[i].isPhantom && sl.l_speakers[i].enabled->get()){
                        int speakerChannel = sl.l_speakers[i].deviceChannel;
                        float spkGain = sl.l_speakers[i].speakerGain->get();
                        if(vs->decorrelateSrc.get()){
                            sample = decorrelation.getOutputBuffer(sl.l_speakers[i].decorrelationOutputIdx)[io.frame()];
                            setOutput(io,speakerChannel,io.frame(),sample * (gains[speakerChannel] / gainScaleFactor) * xFadeGain * spkGain);

                        }else{
                            setOutput(io,speakerChannel,io.frame(),sample * (gains[speakerChannel] / gainScaleFactor) * xFadeGain * spkGain);
                        }
                    }
                }
            }
        }

//        else if(vs->panMethod.get() == 2){ // Snap Source Width
//            float sample = 0.0f;
//            float gainsAccum = 0.0;
//            float gains[speakers.size()];
//            if(!vs->decorrelateSrc.get()){
//                 sample = vs->getSample();
//            }

//            for (int i = 0; i < speakers.size(); i++){
//                gains[i] = 0.0;
//                if(!speakers[i].isPhantom && speakers[i].enabled->get()){
//                    float angleDiff = getAbsAngleDiff(vs->aziInRad,speakers[i].aziInRad);
//                    if(angleDiff <= vs->sourceWidth/2.0f){
//                        gains[i] = 1.0;
//                        gainsAccum += 1.0;
//                    }
//                }
//            }

//            float gainScaleFactor = 1.0;
//            if(vs->scaleSrcWidth.get()){
//                if(!gainsAccum == 0.0){
//                    gainScaleFactor = sqrt(gainsAccum);
//                }
//            }

//            for (int i = 0; i < speakers.size(); i++){

//                if(!speakers[i].isPhantom && speakers[i].enabled->get()){
//                    int speakerChannel = speakers[i].deviceChannel;
//                    //float angleDiff = getAbsAngleDiff(vs->aziInRad,speakers[i].aziInRad);
//                        if(vs->decorrelateSrc.get()){
//                            sample = decorrelation.getOutputBuffer(speakerChannel+ outputBufferOffset)[io.frame()];
//                            setOutput(io,speakerChannel,io.frame(),sample * (gains[i] / gainScaleFactor) * xFadeGain);
//                        }else{
//                            setOutput(io,speakerChannel,io.frame(),sample * (gains[i] / gainScaleFactor) * xFadeGain);
//                        }

//                }

//            }

//        }

        else if(vs->panMethod.get() == 3){ //Snap to Nearest Speaker
            float smallestAngle = M_2PI;
            int smallestAngleSpkIdx = -1;
            int smallestAngleLayerIdx = -1;

            int speakLayerIdx = 0;
            for(SpeakerLayer& sl: layers){

                for (int i = 0; i < sl.l_speakers.size(); i++){
                    if(!sl.l_speakers[i].isPhantom && sl.l_speakers[i].enabled->get()){
                        float angleDiff = getAbsAngleDiff(vs->aziInRad,sl.l_speakers[i].aziInRad) + getAbsAngleDiff(vs->elevation,sl.elevation);
                        if(angleDiff < smallestAngle){
                            smallestAngle = angleDiff;
                            smallestAngleSpkIdx = i;
                            smallestAngleLayerIdx = speakLayerIdx;
                        }
                    }
                }
                speakLayerIdx++;
            }

            if(smallestAngleSpkIdx != -1){
                int speakerChannel = layers[smallestAngleLayerIdx].l_speakers[smallestAngleSpkIdx].deviceChannel;
                float spkGain = layers[smallestAngleLayerIdx].l_speakers[smallestAngleSpkIdx].speakerGain->get();
                float sample = 0.0f;
                if(vs->decorrelateSrc.get()){
                    sample = decorrelation.getOutputBuffer(speakerChannel)[io.frame()];
                }else{
                    sample = vs->getSample(io, inputChannel);
                }
                setOutput(io,speakerChannel,io.frame(),sample * xFadeGain * spkGain);
            }


//            float smallestAngle = M_2PI;
//            int smallestAngleSpkIdx = -1;
//            for (int i = 0; i < speakers.size(); i++){
//                if(!speakers[i].isPhantom && speakers[i].enabled->get()){
//                    float angleDiff = getAbsAngleDiff(vs->aziInRad,speakers[i].aziInRad);
//                    if(angleDiff < smallestAngle){
//                        smallestAngle = angleDiff;
//                        smallestAngleSpkIdx = i;
//                    }
//                }
//            }

//            if(smallestAngleSpkIdx != -1){
//                int speakerChannel = speakers[smallestAngleSpkIdx].deviceChannel;
//                float sample = 0.0f;
//                if(vs->decorrelateSrc.get()){
//                    sample = decorrelation.getOutputBuffer(speakerChannel+ outputBufferOffset)[io.frame()];
//                }else{
//                    sample = vs->getSample();
//                }
//                setOutput(io,speakerChannel,io.frame(),sample * xFadeGain);
//            }

        }else if(vs->panMethod.get() == 4){ //Snap With Fade
//            float smallestAngle = M_2PI;
//            int smallestAngleSpkIdx = -1;
//            for (int i = 0; i < speakers.size(); i++){
//                if(!speakers[i].isPhantom && speakers[i].enabled->get()){
//                    float angleDiff = getAbsAngleDiff(vs->aziInRad,speakers[i].aziInRad);
//                    if(angleDiff < smallestAngle){
//                        smallestAngle = angleDiff;
//                        smallestAngleSpkIdx = i;
//                    }
//                }
//            }

//            if(smallestAngleSpkIdx != -1){
//                int speakerChannel = speakers[smallestAngleSpkIdx].deviceChannel;
//                float sample = 0.0f;
//                float prevGain, currentGain;
//                if(vs->currentSnapChan != speakerChannel){
//                    vs->prevSnapChan = vs->currentSnapChan;
//                    vs->currentSnapChan = speakerChannel;
//                    vs->fadeCounter = 0;
//                }

//                vs->getFadeGains(prevGain,currentGain);

//                if(vs->decorrelateSrc.get()){
//                    sample = decorrelation.getOutputBuffer(speakerChannel+ outputBufferOffset)[io.frame()];
//                }else{
//                    sample = vs->getSample();
//                }
//                setOutput(io,vs->prevSnapChan,io.frame(),sample*prevGain * xFadeGain);
//                setOutput(io,vs->currentSnapChan,io.frame(),sample*currentGain * xFadeGain);
//            }
        }
    }

    void onInit() override {

// ****** 2809 ******
        if(location == 0){

            SpeakerLayer speakerLayer;
            speakerLayer.elevation = 0.0;

            float startingAngle = 170.0f;
            float angleInc = 11.0f;
            float ang;
            for (int i = 0; i < 32; i++){
                int delay = rand() % static_cast<int>(MAX_DELAY + 1);
                ang = startingAngle - (angleInc*i);
                speakerLayer.l_speakers.push_back(SpeakerV(i,ang,0.0,0,5.0,0,delay));
            }

            //-1 for phantom channels (can remove isPhantom and just check -1)
            SpeakerV s(-1, startingAngle+angleInc,0.0,0,5.0,0,0);
            s.isPhantom = true;
            speakerLayer.l_speakers.push_back(s);

            SpeakerV p(-1, ang - angleInc,0.0,0,5.0,0,0);
            p.isPhantom = true;
            speakerLayer.l_speakers.push_back(p);

            layers.push_back(speakerLayer);
        }
// ******  end 2809 ******

// ****** Allosphere ******
        else if (location == 1){

            Speakers alloLayout;
            alloLayout = AlloSphereSpeakerLayout();

            //TODO: Make this automatic
            SpeakerLayer bottom, middle, top;
            bottom.elevation =  -32.5 * float(M_PI) / 180.f;
            middle.elevation = 0.0;
            top.elevation = 41.0 * float(M_PI) / 180.f;

            for(int i = 0; i < alloLayout.size(); i++){

                Speaker spk = alloLayout[i];
                if(spk.elevation == -32.5){
                    //Allosphere azimuth increases clockwise
                    bottom.l_speakers.push_back(SpeakerV(spk.deviceChannel,spk.azimuth* -1.0,spk.elevation,0,5.0,0,0.0));
                }else if(spk.elevation == 0.0){
                    middle.l_speakers.push_back(SpeakerV(spk.deviceChannel,spk.azimuth* -1.0,spk.elevation,0,5.0,0,0.0));
                }else if(spk.elevation == 41.0){
                    top.l_speakers.push_back(SpeakerV(spk.deviceChannel,spk.azimuth* -1.0,spk.elevation,0,5.0,0,0.0));
                }
            }

           layers.push_back(bottom);
           layers.push_back(middle);
           layers.push_back(top);

        }
//****** end Allosphere ******

        initPanner();

        for(SpeakerLayer &sl:layers){
            speakerCount += sl.l_speakers.size();
            for(int i = 0; i < sl.l_speakers.size(); i++){
                //parameterGUI << sl.l_speakers[i].enabled  << sl.l_speakers[i].speakerGain;
                //presets << *sl.l_speakers[i].enabled;
                if((int) sl.l_speakers[i].deviceChannel > highestChannel){
                    highestChannel = sl.l_speakers[i].deviceChannel;
                }

                allSpeakers.push_back(&sl.l_speakers[i]); //Add reference to allSpeakers
            }
        }

        audioIO().channelsOut(highestChannel + 1);

        cout << "channelsOutDevice(): " << audioIO().channelsOutDevice() << endl;

        //cout << "Hightst Channel: " << highestChannel << endl;

        mPeaks = new atomic<float>[highestChannel + 1];

        addSphere(mSpeakerMesh, 1.0, 16, 16);
        mSpeakerMesh.primitive(Mesh::TRIANGLES);

        uint32_t key = 0;
        uint32_t val = 0;

        //TODO: this is for one source only, "ERROR convolution config failed" if > than 2 sources
        // MAXINP set to 64 in zita-convolver.h line 362

        for(int i = 0 ; i < sourcesToDecorrelate.get(); i++){
            vector<uint32_t> values;

            for(SpeakerLayer &sl:layers){
                for(int j = 0; j < sl.l_speakers.size(); j++){
                    if(!sl.l_speakers[j].isPhantom){
                        //Values are from 0, number of speakers -1
                        sl.l_speakers[j].decorrelationOutputIdx = val;
                        values.push_back(val);
                        val++;
                    }
                }
            }
            routingMap.insert(std::pair<uint32_t,vector<uint32_t>>(key,values));
            key++;
        }

        //Prints routing map
//        for(auto it = routingMap.cbegin(); it != routingMap.cend(); ++it){
//            //std::cout << "Decorrelation input Channel: " << it->first << endl;
//            //cout << "Values: " << endl;
//            for(auto sec: it->second){
//                cout<< sec << " ";
//            }
//            cout << endl;
//        }

        decorrelation.configure(audioIO().framesPerBuffer(), routingMap,
                                true, decorrelationSeed, maxJump.get(),phaseFactor.get());


        audioIO().append(soundFileRecorder);


        VirtualSource *vs = sources[0];

        //Create some events
        auto *event1 = new Event("Event 1");
        auto *event2 = new Event("Event 2");
        auto *event3 = new Event("Event 3");
        auto *eventSequencer = new Event("Sequence");

        //Add components to event1
        event1->addBPF(&vs->sourceGain,0.0,{1.0,1.0,1.0,1.0,0.5,1.0});
        event1->addBPF(&vs->centerAzi,0.0,{2.0,1.0,1.0,1.0,0.0,1.0});
        event1->setParameter(&vs->enabled,1.0);
        event1->setMenu(&vs->fileMenu,"shortCount.wav");
        event1->setMenu(&vs->panMethod,"LBAP");

        //Add components to event2
        event2->setMenu(&vs->panMethod,"Source Spread");
        event2->setParameter(&vs->scaleSrcWidth,1.0);
        event2->addBPF(&vs->sourceWidth,0.0,{2.0,3.0});

        //Create some speaker groups
        auto *speakerGroup = new SpeakerGroup("Group 1");
        auto *speakerGroup2 = new SpeakerGroup("Group 2");

        speakerGroup->addSpeakersByChannel({0,2,4,6,8,10});
        speakerGroup2->addSpeakersByChannel({1,3,5,7,9,11,13,15,17});

        //Register the speaker groups
        speakerGroups.push_back(speakerGroup);
        speakerGroups.push_back(speakerGroup2);

        //Add gain automation of speaker groups 1 and 2 to event 3
        event3->addBPF(&speakerGroup->gain,1.0,{0.0,5.0});
        event3->addBPF(&speakerGroup2->gain,1.0,{0.0,3.0,1.0,3.0});

        //Add events to eventSequencer with the start time in seconds
        eventSequencer->addEvent(event1,0.0);
        eventSequencer->addEvent(event2,4.0);
        eventSequencer->addEvent(event3,5.0);

        //Register the events
        events.push_back(event1);
        events.push_back(event2);
        events.push_back(event3);
        events.push_back(eventSequencer);

    }

    void onCreate() override {

        nav().pos(0, 1, 20);

        font.load("src/data/VeraMono.ttf", 28, 1024);
        font.alignCenter();
        font.write(fontMesh, "1", 0.2f);

        imguiInit();
        parameterGUI.init(0,0,false);

        audioIO().start();
    }

    void onAnimate( double dt) override {
        navControl().active(!parameterGUI.usingInput());
    }

    virtual void onSound(AudioIOData &io) override {

        static unsigned int t = 0;
        float enabledSources = 0.0f;
        gam::Sync::master().spu(audioIO().fps());

        if(soundOn.get()){

                //Only reconfigure if method has changed
                if(configureDecorrelation.get()){
                    configureDecorrelation.set(0.0);
                    if(!decorrelationMethod.get()){
                        //Kendall Method
                        decorrelation.configure(audioIO().framesPerBuffer(), routingMap,
                                                true, decorrelationSeed, maxJump.get(),phaseFactor.get());
                    }else {
                        //Zotter Method
                        decorrelation.configureDeterministic(audioIO().framesPerBuffer(), routingMap, true, decorrelationSeed, deltaFreq, maxFreqDev, maxTau, startPhase, phaseDev);
                    }
                    //cout << "IR lenghts: " << decorrelation.getSize() << endl;
                }

                for(VirtualSource *v: sources){
                    if(v->enabled && v->decorrelateSrc.get()){
                        auto inBuffer = decorrelation.getInputBuffer(v->vsBundle.bundleIndex());
                        for(int i = 0; i < BLOCK_SIZE; i++){
                            int inputChannel = v->inputChannel.get();
                            inBuffer[i] = v->getSample(io,inputChannel);
                        }
                    }
                    if(sourcesToDecorrelate.get() == v->vsBundle.bundleIndex()+1){
                         break;
                    }
                }
                decorrelation.processBuffer(); //Always processing buffer, change if needed
            
            
            while (io()) {

                ++t;

                for(Event *e: events){
                    e->processEvent(t);
                }

                if(sampleWise.get()){
                        for(VirtualSource *v: sources){
                            if(v->enabled){
                                enabledSources += 1.0f;
                                v->updatePosition(t);
                                renderSample(io,v);
                            }
                        }
                }
            }
        }

//        for(SpeakerLayer sl:layers){
//            for (int i = 0; i < sl.l_speakers.size(); i++) {
//                mPeaks[sl.l_speakers[i].deviceChannel].store(0);
//            }
//        }
        for (int i = 0; i < highestChannel+1; i++) {
            mPeaks[i].store(0);
        }

        for(SpeakerLayer sl:layers){
            for (int speaker = 0; speaker < sl.l_speakers.size(); speaker++) {
                if(!sl.l_speakers[speaker].isPhantom){
                    float rms = 0;
                    int deviceChannel = sl.l_speakers[speaker].deviceChannel;

                    for (int i = 0; i < io.framesPerBuffer(); i++) {
                        if(deviceChannel < io.channelsOut()) {
                            float sample = io.out(deviceChannel, i);
                            rms += sample * sample;
                        }
                    }
                    rms = sqrt(rms/io.framesPerBuffer());
                    mPeaks[deviceChannel].store(rms);
                }
            }
        }

        if(stereoOutput.get()){

            float stereoScale = stereoOutputGain.get();

            for(SpeakerLayer sl:layers){
                for (int speaker = 0; speaker < sl.l_speakers.size(); speaker++) {
                    if(!sl.l_speakers[speaker].isPhantom){
                        int deviceChannel = sl.l_speakers[speaker].deviceChannel;
                        if(deviceChannel > 1 && deviceChannel < io.channelsOut()){
                            for (int i = 0; i < io.framesPerBuffer(); i++) {
                                if(deviceChannel % 2 == 0){
                                    io.out(0,i) += io.out(deviceChannel, i) * stereoScale;
                                }else{
                                    io.out(1,i) += io.out(deviceChannel, i) * stereoScale;
                                }
                            }
                        }
                    }
                }
            }
        }

    }

    virtual void onDraw(Graphics &g) override {

        g.clear(0);
        g.blending(true);
        g.blendModeAdd();

        g.pushMatrix();
        Mesh lineMesh;
        lineMesh.vertex(0.0,0.0, 10.0);
        lineMesh.vertex(0.0,0.0, -10.0);
        lineMesh.index(0);
        lineMesh.index(1);
        lineMesh.primitive(Mesh::LINES);
        g.color(1);
        g.draw(lineMesh);
        g.popMatrix();

        static int t = 0;
        t++;
        //Draw the sources
        for(VirtualSource *v: sources){
            Vec3d pos = ambiSphericalToOGLCart(v->aziInRad, v->elevation,radius);
            g.pushMatrix();
            g.translate(pos);

            if(drawLabels.get()){
                //Draw Source Index
                g.pushMatrix();
                g.color(1);
               // g.color(0.0,0.0,1.0);
                g.translate(0.0,0.3,0.0);
                font.write(fontMesh,std::to_string(v->vsBundle.bundleIndex()).c_str(),0.2f);
                g.texture();
                font.tex.bind();
                g.draw(fontMesh);
                font.tex.unbind();
                g.popMatrix();
            }

            g.scale(0.1);
            g.color(0.0,0.0, 1.0, 1.0);
            g.draw(mSpeakerMesh);
            g.popMatrix();

//            g.pushMatrix();
//            g.translate(v->getSamplePlayerPhase(),2.0 + v->vsBundle.bundleIndex()*0.25,0.0);
//            g.scale(0.3);
//            g.draw(mSpeakerMesh);
//            g.popMatrix();

            // Draw line
            Mesh lineMesh;
//            lineMesh.vertex(0.0,0.0, 0.0);
//            lineMesh.vertex(pos.x,0.0, pos.z);
//            lineMesh.vertex(pos);
//            lineMesh.index(0);
//            lineMesh.index(1);
//            lineMesh.index(1);
//            lineMesh.index(2);
//            lineMesh.index(2);
//            lineMesh.index(0);
//            lineMesh.primitive(Mesh::LINES);
//            g.color(1);
//            g.draw(lineMesh);
            lineMesh.vertex(0.0,0.0, 0.0);
            lineMesh.vertex(pos);
            lineMesh.index(0);
            lineMesh.index(1);
            lineMesh.primitive(Mesh::LINES);
            g.color(1);
            g.draw(lineMesh);
        }

        //Draw the speakers
        for(SpeakerLayer sl: layers){
            for(int i = 0; i < sl.l_speakers.size(); ++i){
                int devChan = sl.l_speakers[i].deviceChannel;
                g.pushMatrix();
                g.translate(sl.l_speakers[i].vecGraphics());

                if(drawLabels.get()){
                    //Draw Speaker Channel
                    g.pushMatrix();
                    g.translate(0.0,0.1,0.0);
                    if(!sl.l_speakers[i].isPhantom){
                        font.write(fontMesh,sl.l_speakers[i].deviceChannelString.c_str(),0.2f);
                    }else{
                        font.write(fontMesh,"P",0.2f);
                    }
                    g.texture();
                    font.tex.bind();
                    g.draw(fontMesh);
                    font.tex.unbind();
                    g.popMatrix();
                }

                float peak = 0.0;
                if(!sl.l_speakers[i].isPhantom){
                    peak = mPeaks[devChan].load();
                }
                g.scale(0.04 + peak * 6);

//                if(sl.l_speakers[i].isPhantom){
//                    g.color(0.0,1.0,0.0);
//                }else if(devChan == 0){
//                    g.color(1.0,0.0,0.0);
//                }else if(devChan == 1){
//                    g.color(0.0,0.0,1.0);
//                }else if(!sl.l_speakers[i].enabled->get()){
//                    g.color(0.05,0.05,0.05);
//                }else{
//                    g.color(1);
//                }
                g.color(1);
                g.draw(mSpeakerMesh);
                g.popMatrix();
            }
        }
        //parameterGUI.draw(g);


        imguiBeginFrame();

        if(showLoudspeakerGroups){
            ParameterGUI::beginPanel("Speaker Groups");
            if (ImGui::Button("Close")){
                showLoudspeakerGroups = false;
            }
            ImGui::Separator();
            for(SpeakerGroup *sg: speakerGroups){
                ImGui::Text(sg->groupName.c_str());
                ParameterGUI::drawParameterBool(&sg->enable);
                ParameterGUI::drawParameter(&sg->gain);
                ImGui::Separator();
            }
            ParameterGUI::endPanel();
        }

        if(showEvents){
            ParameterGUI::beginPanel("Events");
            if (ImGui::Button("Close")){
                showEvents = false;
            }
            ImGui::Separator();
            for(Event *eg: events){
                ParameterGUI::drawTrigger(&eg->triggerEvent);
            }
            ParameterGUI::endPanel();
        }

        if(showRecorder){
            ParameterGUI::beginPanel("Record");
            if (ImGui::Button("Close")){
                showRecorder = false;
            }
            ImGui::Separator();
            SoundFileRecordGUI::drawRecorderWidget(&soundFileRecorder, audioIO().framesPerSecond(), audioIO().channelsOut());
            ParameterGUI::endPanel();
        }

        if(showXFade){
            ParameterGUI::beginPanel("Cross Fade Ch. 1 and Ch. 2");
            if (ImGui::Button("Close")){
                showXFade = false;
            }
            ImGui::Separator();
            ParameterGUI::drawParameterBool(&xFadeCh1_2);
            ParameterGUI::drawParameter(&xFadeValue);
            ParameterGUI::endPanel();
        }

        if(showDecorrelation){
            ParameterGUI::beginPanel("Decorrelation");
            if (ImGui::Button("Close")){
                showDecorrelation = false;
            }
            ImGui::Separator();
            ParameterGUI::drawMenu(&decorrelationMethod);
            ParameterGUI::drawTrigger(&generateRandDecorSeed);
            ParameterGUI::drawParameter(&maxJump);
            ParameterGUI::drawParameter(&phaseFactor);
            ParameterGUI::drawParameter(&deltaFreq);
            ParameterGUI::drawParameter(&maxFreqDev);
            ParameterGUI::drawParameter(&maxTau);
            ParameterGUI::drawParameter(&startPhase);
            ParameterGUI::drawParameter(&phaseDev);
            ParameterGUI::endPanel();
        }

        if(showSetAllSources){
            ParameterGUI::beginPanel("Set All Sources");
            if (ImGui::Button("Close")){
                showSetAllSources = false;
            }

            ImGui::Separator();
            ParameterGUI::drawParameterBool(&setAllEnabled);
            ParameterGUI::drawParameterBool(&setAllMute);
            ParameterGUI::drawParameterBool(&setAllDecorrelate);
            ParameterGUI::drawParameter(&setAllGain);
            ParameterGUI::drawMenu(&setAllPanMethod);
            ParameterGUI::drawMenu(&setAllPosUpdate);
            ParameterGUI::drawMenu(&setAllSourceSounds);
            ParameterGUI::drawParameterInt(&setAllInputChannel,"");
            ParameterGUI::drawMenu(&setAllSoundFileIdx);

            ImGui::Separator();
            if(ImGui::TreeNode("Sample Players")){
                ParameterGUI::drawParameter(&setPlayerPhase);
                ParameterGUI::drawParameter(&playerRateMultiplier);
                ParameterGUI::drawParameterBool(&useRateMultiplier);

                ImGui::Separator();

                ImGui::Text("Source Phases");
                ImVec2 size;
                size.x = 200;
                size.y = 100;

                float values[NUM_SOURCES] = {};

                for(int i = 0; i < NUM_SOURCES; i++){
                    VirtualSource *vs = sources[i];
                    float value = 0.0;
                    if(vs->enabled.get()){
                        value = sources[i]->getSamplePlayerPhase();
                    }
                    values[i] = value;
                }

                ImGui::PlotHistogram("##values", values, IM_ARRAYSIZE(values), 0, NULL, 0.0f, 1.0f, size);
                ImGui::TreePop();
            }
            ImGui::Separator();
            if(ImGui::TreeNode("Positions")){
                ParameterGUI::drawParameter(&setAllAzimuth);
                ParameterGUI::drawParameter(&setAllAzimuthOffsetScale);
                ParameterGUI::drawParameter(&setAllElevation);
                ParameterGUI::drawParameter(&setAllEleOffsetScale);

                //ParameterGUI::drawTrigger(&resetPosOscPhase);
                ImGui::TreePop();
            }
            ParameterGUI::endPanel();
        }


        if (ImGui::BeginMainMenuBar()){

            if(ImGui::BeginMenu("File")){
                if(ImGui::MenuItem("Quit")){
                    exit(0);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window")){

                if (ImGui::MenuItem("Virtual Sources")) {
                    showVirtualSources = true;
                }
                if (ImGui::MenuItem("Events")){
                    showEvents = true;
                }
                if (ImGui::MenuItem("Speaker Groups")){
                    showLoudspeakerGroups = true;
                }
                if (ImGui::MenuItem("Loudspeakers")){
                    showLoudspeakers = true;
                }
                if (ImGui::MenuItem("Recording")){
                    showRecorder = true;
                }
                if (ImGui::MenuItem("Set All Sources")){
                    showSetAllSources = true;
                }
                if (ImGui::MenuItem("Cross Fade Ch. 1 and 2")){
                    showXFade = true;
                }
                if (ImGui::MenuItem("Decorrelation")){
                    showDecorrelation = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ParameterGUI::beginPanel("Main");
        ParameterGUI::drawParameterBool(&soundOn);
        ParameterGUI::drawParameter(&masterGain);
        ParameterGUI::drawParameterBool(&stereoOutput);
        ParameterGUI::drawParameter(&stereoOutputGain);
        ParameterGUI::endPanel();

        if(showLoudspeakers){
            ParameterGUI::beginPanel("Loudspeakers");
            if (ImGui::Button("Close")){
                showLoudspeakers = false;
            }
            ImGui::Separator();

            ParameterGUI::drawMenu(&speakerDensity);
            ParameterGUI::drawParameterBool(&drawLabels);
            for(SpeakerLayer sl: layers){
                for(SpeakerV sv: sl.l_speakers){
                    ParameterGUI::drawParameterBool(sv.enabled);
                    ParameterGUI::drawParameter(sv.speakerGain);
                }
            }
            ParameterGUI::endPanel();
        }

        if(showVirtualSources){
            ParameterGUI::beginPanel("Virtual Sources");

            if (ImGui::Button("Close")){
                showVirtualSources = false;
            }
            ImGui::Separator();
            ParameterGUI::drawParameterInt(&displaySource,"");

            VirtualSource *src = sources[displaySource.get()];
            ParameterGUI::drawParameterBool(&src->enabled);
            ParameterGUI::drawParameterBool(&src->mute);
            ParameterGUI::drawParameterBool(&src->decorrelateSrc);

            ParameterGUI::drawParameter(&src->sourceGain);

            ParameterGUI::drawMenu(&src->panMethod);
            ParameterGUI::drawMenu(&src->positionUpdate);
            ParameterGUI::drawMenu(&src->sourceSound);
            ParameterGUI::drawParameterInt(&src->inputChannel,"");
            ParameterGUI::drawMenu(&src->fileMenu);

            ImGui::Separator();

            if(ImGui::TreeNode("Sample Player")){
                ParameterGUI::drawParameter(&src->samplePlayerRate);
                ParameterGUI::drawParameter(&src->soundFileStartPhase);
                ParameterGUI::drawParameter(&src->soundFileDuration);
                ImGui::TreePop();
            }

            ImGui::Separator();

            if(ImGui::TreeNode("Position")){
                ParameterGUI::drawParameter(&src->centerAzi);
                ParameterGUI::drawParameter(&src->aziOffset);
                ParameterGUI::drawParameter(&src->aziOffsetScale);
                ParameterGUI::drawParameter(&src->centerEle);
                ParameterGUI::drawParameter(&src->eleOffset);
                ParameterGUI::drawParameter(&src->eleOffsetScale);
                ImGui::TreePop();
            }

            ImGui::Separator();
            if(ImGui::TreeNode("Position Update: Moving")){
                ParameterGUI::drawParameter(&src->angularFreq);
                ParameterGUI::drawParameter(&src->angFreqCycles);
                ParameterGUI::drawParameter(&src->angFreqOffset);
                ParameterGUI::drawParameterInt(&src->angFreqCyclesMult,"");
                ParameterGUI::drawTrigger(&src->loopLengthToRotFreq);
                ImGui::TreePop();
            }

            ImGui::Separator();

            if(ImGui::TreeNode("Position Update: Sine")){
                ParameterGUI::drawParameter(&src->posOscFreq);
                ParameterGUI::drawParameter(&src->posOscAmp);
                ParameterGUI::drawParameter(&src->posOscPhase);
                ImGui::TreePop();
            }

            ImGui::Separator();
            if(ImGui::TreeNode("Sine, Saw, Square")){
                ParameterGUI::drawParameter(&src->oscFreq);
                ImGui::TreePop();
            }

            ImGui::Separator();
            if(ImGui::TreeNode("Source Spread")){
                ParameterGUI::drawParameter(&src->sourceWidth);
                ParameterGUI::drawParameterBool(&src->scaleSrcWidth);
                ImGui::TreePop();
            }

            ImGui::Separator();
            if(ImGui::TreeNode("Windowed Phase")){
                //ParameterGUI::drawParameterBool(&src->wp.printInfo);
                ParameterGUI::drawParameterBool(&src->wp.loopWindow);
                ParameterGUI::drawParameterInt(&src->wp.windowStart,"");
                ParameterGUI::drawParameterInt(&src->wp.windowLength,"");
                ParameterGUI::drawParameterInt(&src->wp.increment,"");
                ImGui::TreePop();
            }
            ParameterGUI::endPanel();
        }

        imguiEndFrame();
        imguiDraw();

    }

    void printConfiguration(){
        VirtualSource *vs = sources[0];
        cout << "----CONFIGURATION----\n"
             << "PanningMethod: " + vs->panMethod.getCurrent() + "\n"
             << "Position Update: " + vs->positionUpdate.getCurrent() + "\n"
             << "Source Sound: " + vs->sourceSound.getCurrent() + "\n"
             << "Sound File: " + vs->fileMenu.getCurrent() + "\n"
             << "Source Gain: " + to_string(vs->sourceGain.get()) + "\n"
             << "Azimuth: " + to_string(vs->aziInRad.get()) + "\n"
             << "AngFreqCycles: " + to_string(vs->angFreqCycles.get()) + "\n"
             << "Osc Freq: " + to_string(vs->oscFreq.get()) + "\n"
             << "Source Width: " + to_string(vs->sourceWidth.get()) + "\n"
             << "Fade Duration: " + to_string(vs->fadeDuration.get()) + "\n"
             << "\n"

             << "Decorrelate: " + to_string(setAllDecorrelate.get()) + "\n"
             << "Decorrelation Method: " + decorrelationMethod.getCurrent() + "\n"
             << endl;

    }

    bool onKeyDown(const Keyboard &k) override {
        switch (k.key()) {
        case '1':
            printConfiguration();
            break;
        default:
            break;
        }
    }
};


int main(){
    MyApp app;
    AudioDevice::printAll();
    AudioDevice dev = AudioDevice("Aggregate Device").id();
    //AudioDevice dev = AudioDevice("Soundflower (2ch)").id();

//    app.configureAudio(dev, SAMPLE_RATE, BLOCK_SIZE, 2, 0);
    app.configureAudio(dev, SAMPLE_RATE, BLOCK_SIZE, 2, INPUT_CHANNELS);

    AudioDevice inDevice = AudioDevice("Soundflower (2ch)").id();
    app.audioIO().deviceIn(inDevice);


    // Use this for sphere
    //    app.initAudio(44100, BLOCK_SIZE, -1, -1, AudioDevice("ECHO X5").id());

    app.start();
    return 0;
}
