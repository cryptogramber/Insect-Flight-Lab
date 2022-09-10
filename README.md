## Visual-Stimulation-Lab

**This code is a mess (sorry!) and is not actively developed.**

Series of tools designed for observing and experimenting with the mechanics of insect flight. Includes a tethered insect flight simulator which combines a closed loop visual feedback system with an optical transducer that records torque produced by yaw motions – this allows for (soft) real time external control of the relationship between insect yaw motions and resulting movement of the insect’s visual field. I was interested in the extent to which feedback gain between torque (yaw) and image motion determines an animal’s ability to track a visual stimulus, along with the ability of an animal to adapt to different gains (including negative gain) applied to the feedback loop. The latter investigates the potential for natural plasticity in the visual flight control circuit.

### Torque Sensor Components

![torque_render](https://user-images.githubusercontent.com/83111496/189475157-06b32135-a0c0-4642-99b2-5784fe1fd5d9.png)

- 1x Tungsten Wire/Rod
- 5x Miniature Screws (2x top, 2x bottom, 1x mount)
- 2x Precision Ball Bearings (SD 3mm, OD 10mm, Width 4mm) (http://www.mcmaster.com/#7804k128/=cypl5s)
- 1x Brass Rod (1/4, 6.35mm diameter) (http://www.acehardwareoutlet.com…SKU=5391016)
- 1x OSI Optoelectronics Dual Photodiode
- 1x 940nm IR LED (http://www.radioshack.com…2062565)
- 1x AD734 (10MHz, 4-Quadrant Multiplier/Divider) (http://www.analog.com…product.html)
- 1x OP467 (Quad Op-Amp) (http://www.analog.com…product.html)
- AD827 (Dual Op-Amp) (http://www.analog.com…product.html)
- 2x 100k resistors
- 2x 5k resistors
- 2x capacitors
- +/-12V Voltage Source

### Insect Virtual Flight Arena/Rear Projection System Components

![torquearena](https://user-images.githubusercontent.com/83111496/189475163-1a2f11ed-5e9a-45d8-aab1-c2d9d0232ffb.png | width=300)

- Curved rear projection screen (sheet of mylar from your local art store works fine)
- VSL Software (Win & OSX Versions)
    - Version 1: Torque Sensor Alpha → Level Shifter → Arduino → VSL v1.0 in Processing
    - Version 2: Torque Sensor Alpha → Level Shifter → Teensy → VSL v2.0 in OpenGL/C
    - Version 3: Torque Sensor Beta → NI DAQ Board Input A → MATLAB → Torque Sensor Beta → NI DAQ Board Input B → VSL v3.0 in OpenGL/C
    - Version 4: Torque Sensor Beta → NI DAQ Board → VSL v4.0 in OpenGL/C with (optional) MATLAB export
- EAGLE (only necessary if you want to produce the Torque Sensor interface PCB)
- (optional, for data processing) MATLAB
- 80-20 was used to construct the arena frame, with a curved sheet of mylar acting as the rear projection screen
- TI pico projector (http://www.ti.com/tool/dlp1picokit) was used to project the visual stimulus onto the convex surface of the curved mylar
- (optional, but awesome) low-pass filter/instrumentation amp from Alligator Technologies to filter out the wingbeat frequency
- NI-DAQ board for data collection (in my experience, the more expensive the DAQ board, the more issues you will have – be wary)
