#include <iostream>
#include <fstream>
#include <math.h>
#include "analyze.h"

Analyze::Analyze(string audioFilename, string exerciseFilename) {
	getExercise(exerciseFilename);
	getPitches(audioFilename);
	smoothPitches();

	detected.allocate(exerLength);
}

Analyze::~Analyze() {
	delete exercise;
	detected.~realvec();
	pitchList.~realvec();
}

void Analyze::calcDurations() {
	int prevSamp=0;
	mrs_real avg1, avg2;
	mrs_real variance1, variance2;

	int i, j;
	int detectedIndex=0;

	int next=0;
	float AVERAGE_OVER = 5.0;
	prevSamp=0;
	for (i=AVERAGE_OVER; i<numPitches-AVERAGE_OVER; i++) {
		avg1=0.0;
		avg2=0.0;
		variance1=0.0;
		variance2=0.0;
		for (j=0; j<AVERAGE_OVER; j++) {
			if (j < (AVERAGE_OVER-1) )
				variance1+= fabs(pitchList(i-j-1) - pitchList(i-j-2));
			if (j < (AVERAGE_OVER-1) )
				variance2+= fabs(pitchList(i+j) - pitchList(i+j+1));
		}
		avg1 = findMedian( i-AVERAGE_OVER-1, AVERAGE_OVER, pitchList );
		avg2 = findMedian( i, AVERAGE_OVER, pitchList );
		variance1 = variance1 / (AVERAGE_OVER-1);
		variance2 = variance2 / (AVERAGE_OVER-1);

		if (fabs(avg1-avg2) > 0.6) {
			if (i>prevSamp+3*(AVERAGE_OVER)) next=i;
		}
		if (next>0) {
			if ((variance1<0.4)&&(variance2<0.4)) {
				prevSamp = int(i);
	//			cout<<"---------------------------"<<endl;
				detected(detectedIndex)=i;
				detectedIndex+=2;
				next=0;
			}
		}
//		cout<<i<<" "<<pitchList(i)<<"   "<<avg1<<" "<<avg2<<"   "<<i - prevSamp<<"   "<<"   "<<variance1<<" "<<variance2<<endl;
	}
	detected(exerLength-2) = numPitches;
}

void Analyze::getPitches(string audioFilename) {
  MarSystemManager mng;
  MarSystem* pnet = mng.create("Series", "pnet");

  pnet->addMarSystem(mng.create("SoundFileSource", "src"));
	pnet->addMarSystem(mng.create("ShiftInput", "sfi"));
  pnet->updctrl("SoundFileSource/src/mrs_string/filename", audioFilename);
  pnet->addMarSystem(mng.create("PitchPraat", "pitch")); 
  // pnet->addMarSystem(mng.create("PitchSACF", "pitch")); 
  pnet->addMarSystem(mng.create("RealvecSink", "rvSink")); 

  mrs_real lowPitch = 36;
  mrs_real highPitch = 79;
  mrs_real lowFreq = pitch2hertz(lowPitch);
  mrs_real highFreq = pitch2hertz(highPitch);

  mrs_natural lowSamples = 
     hertz2samples(highFreq, pnet->getctrl("SoundFileSource/src/mrs_real/osrate")->toReal());
  mrs_natural highSamples = 
     hertz2samples(lowFreq, pnet->getctrl("SoundFileSource/src/mrs_real/osrate")->toReal());
 
  pnet->updctrl("PitchPraat/pitch/mrs_natural/lowSamples", lowSamples);
  pnet->updctrl("PitchPraat/pitch/mrs_natural/highSamples", highSamples);
  
  //  The window should be just long
  //  enough to contain three periods (for pitch detection) 
  //  of MinimumPitch. E.g. if MinimumPitch is 75 Hz, the window length
  //  is 40 ms and padded with zeros to reach a power of two.
  mrs_real windowSize = 3/lowPitch*pnet->getctrl("SoundFileSource/src/mrs_real/osrate")->toReal();
  pnet->updctrl("mrs_natural/inSamples", 512);
	// pnet->updctrl("ShiftInput/sfi/mrs_natural/Decimation", 256);
	pnet->updctrl("ShiftInput/sfi/mrs_natural/WindowSize", powerOfTwo(windowSize));
	//pnet->updctrl("ShiftInput/sfi/mrs_natural/WindowSize", 1024);

  while (pnet->getctrl("SoundFileSource/src/mrs_bool/notEmpty")->toBool())
   pnet->tick();

	realvec data = pnet->getctrl("RealvecSink/rvSink/mrs_realvec/data")->toVec();
   for (mrs_natural i=1; i<data.getSize();i+=2)
	   data(i) = samples2hertz(data(i), pnet->getctrl("SoundFileSource/src/mrs_real/osrate")->toReal());
   
   pnet->updctrl("RealvecSink/rvSink/mrs_bool/done", true); 



// my addition to the marsyasTest pitch stuff:
  numPitches = data.getSize()/2;
	pitchList.allocate(numPitches);
	for (int i=0; i<numPitches; i++) {
		if ( data(2*i+1)>0 )
			pitchList(i) = hertz2pitch( data(2*i+1) );
		else
			pitchList(i) = 0;
	}
	delete pnet;
}

void Analyze::getExercise(string exerciseFilename) {
	ifstream infile;	
	infile.open( exerciseFilename.c_str() );
	infile >> exerLength;
	exercise = (int*) malloc(exerLength*sizeof(int));

	int i=0;
	while ( infile>>exercise[i] ) {
		i++;
	}
	infile.close();
}

// smooth out 0s.
void Analyze::smoothPitches() {
	int i,j,k;
	float prevPitch;
	i=0;
	while (i<numPitches) {
		if (pitchList(i)==0) {
			j=i;
			while (true) {
				if (j>=numPitches) break;
				if (pitchList(j)>0) {
					if (i==0)
						prevPitch=0;
						else
						prevPitch=pitchList(i-1);
					for (k=i; k<j; k++)	{
						pitchList(k)=prevPitch;
					}
					break;
				}
				j++;
			}
			i=j;
		}
		i++;
	}


	ofstream file;
	file.open("notepitches.txt");
	for (i=0; i<numPitches; i++)
		file<<pitchList(i)<<endl;
	file.close();
}

mrs_real Analyze::findMedian(int start, int length, realvec array) {
//	cout<<start<<" "<<length<<endl;
	if ( !(length>0) ) return 0;
	mrs_real toReturn;
	realvec myArray;
	myArray.allocate(length);
	for (int i=0; i<length; i++) {
		myArray(i) = array(start+i);
	}
	toReturn = myArray.median();

	myArray.~realvec();
	return toReturn;
}

void Analyze::calcNotes(){

	int i, j;

	//first pass of average pitch
	int start, len;
	float sampSum;
	for (i=0; i<exerLength; i=i+2) {
		start = detected(i);
		len = detected(i+2)-start;
		sampSum=0.0;
		detected(i+1) = findMedian(start, len, pitchList);
	}

	//second pass
	int sampCount;
	float oldAverage;
	for (i=0; i<exerLength; i=i+2) {
		start = detected(i);
		len = detected(i+2)-start;
		sampSum=0.0;
		sampCount=0;
		oldAverage=detected(i+1);
		for (j=start; j<start+len; j++) {
			if ( fabs( pitchList(j)-oldAverage) < 1 ) {
				sampSum+=pitchList(j);
				sampCount++;
			}
		}
		if (sampCount>0)
			detected(i+1)=sampSum/sampCount;

		if (detected(i+1)==0)
			detected(i+1) = 73;  // for display in testing
	}

	for (i=0; i<exerLength; i=i+2) {
		cout<<detected(i)<<" "<<detected(i+1)<<endl;
	}	

	for (i=0; i<exerLength; i=i+2) {
		if ( (exercise[i]>0)&&(detected(i+1)>0))
			detected(i+1) = exercise[i+2]/detected(i+1);
		else
			detected(i+1) = 1;
	}	

	for (i=0; i<exerLength; i=i+2) {
//		cout<<detected[i]*512.0/44100.0<<" "<<detected[i+1]<<endl;
	}	

}


