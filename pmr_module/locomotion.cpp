/*
    This file is part of PMR-Firmware.

    PMR-Firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PMR-Firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PMR-Firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "locomotion.h"

Locomotion::Locomotion(Communication* com, HardwareControl* hc, PMRTopology* topology)
{
    active = false;
    forward = true;
    locomotion = WALK;
    locomotionGather = false;
    sampling = 30;  //set one angle every x milliseconds
    lastSample = 0;

    //########sine generator parameters#########
    sine_amplitude = 37;
    sine_frequency = 0.5;
    sine_phase = 130;
    //##########################################

    Locomotion::com = com;
    Locomotion::hc = hc;
    Locomotion::topology = topology;
}

/* clock, must be called periodically from loop()
*/
void Locomotion::tick()
{
    if(active) {
        if(millis() > lastSample + sampling) {
            lastSample = millis();
            switch(locomotion) {
                case WALK: {
                    moveSinusoidal();
                } break; 
                case ROLL: {
                    moveRoll();
                } break;
                default: {
                    moveSinusoidal();
                } break;
            }
        }
    }
}

/* starts locomotion
*/
void Locomotion::start()
{
    active = true;
}

/* stops locomotion
*/
void Locomotion::stop()
{
    active = false;
}

/* sets locomotion type to walk, caterpillar forward moving
*/
void Locomotion::walk()
{
    Serial.println("Locomotion type: walk (standard)");
    locomotion = WALK;
}

/* sets locomotion type to roll, sideward rolling, only possible with sufficient number of pitch AND yaw joints
*/
void Locomotion::roll()
{
    Serial.println("Locomotion type: roll");
    locomotion = ROLL;
}

/* inverts the moving direction between forward and backward. Returns true if forward, false else.
*/
bool Locomotion::changeDirection()
{
    forward = !forward;
    return forward;
}

void Locomotion::setFrequency(float frequency)
{
    sine_frequency = frequency;
}
void Locomotion::setAmplitude(float amplitude)
{
    sine_amplitude = amplitude;
}
void Locomotion::setPhase(float phase)
{
    sine_phase = phase;
}
void Locomotion::setSampling(unsigned int sampling)
{
    Locomotion::sampling = sampling;
}

/* Enables/Disables gathering of joint angle values.
   Values are printed directly to the hosts serial interface.
*/
void Locomotion::gatherAngles(bool gather)
{
    locomotionGather = gather;
}

/* Output is angle in degree
*/
float Locomotion::sineFunction(float amp, float phase, float freq){
    float result = sin((millis()/(float)1000*2*PI)*freq+(phase/360*2*PI))*amp;
    return result;
}

/* generates an array for one locomotion-step using a sinusoidal generator function for a specific number of joints
*/
float* Locomotion::calculateNextJointPositions(int numOfJoints, bool forward){
    int dir = forward ? 1 : -1;
    for(int i=0; i<numOfJoints; i++){
        locomotionAngles[i] = sineFunction(sine_amplitude, sine_phase*(i*dir), sine_frequency);
    }      
    return locomotionAngles;
}

// generates sinusoidal locomotion in forward or backward direction
void Locomotion::moveSinusoidal()
{
    float* angles = calculateNextJointPositions(topology->getPitchingJointsCount(), forward);
    byte modulesCount = topology->getModulesCount();
  
    byte currentPitchingJoint = 0;
    if(locomotionGather) {Serial.print("locomotion pattern:");}
    for(int i=0; i<modulesCount; i++){
        if(topology->getOrientation(i)){  
            if(locomotionGather){        
                if(i>0) {Serial.print(" ");}
                if(i==(modulesCount-1)) {
                    Serial.println(angles[i]);
                }else{
                    Serial.print(angles[i]);
                }
            }        
            if(ADRESS == topology->getAdress(i)){
                hc->setAngle(static_cast<int>(angles[i]));
            }else{
                com->sendDownstream(topology->getAdress(i), SET_ANGLE, (static_cast<byte>(angles[currentPitchingJoint]+128)));
            }
            currentPitchingJoint++;
        }
    }    
}

// generates rolling locomotion in forward or backward direction
void Locomotion::moveRoll()
{
    int dir = forward ? 1 : -1;
    short int rotate = 1;
    for(int i=0; i<topology->getModulesCount(); i++){
        if(topology->getOrientation(i)){
            if(ADRESS == topology->getAdress(i)){
                hc->setServoPosition(static_cast<int>(sineFunction(sine_amplitude, 0, sine_frequency)));
            }else{
                com->sendDownstream(topology->getAdress(i), SET_ANGLE, (static_cast<byte>(rotate * sineFunction(sine_amplitude, 0, sine_frequency)+128)));
            }
        }else{
            com->sendDownstream(topology->getAdress(i), SET_ANGLE, (static_cast<byte>(rotate * sineFunction(sine_amplitude, sine_phase, sine_frequency)+128)));
        rotate = rotate*-1;
        }
    }
}