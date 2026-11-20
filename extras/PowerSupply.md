# Power supply, load switches and servo current

This page documents a series of oscilloscope measurements made in November 2026 on the
Servo-2, Servo-3 and Servo-6 decoder boards. The immediate reason was that the Servo-3
board sometimes reset while an Uhlenbrock servo was moving. The measurements that follow identify the
conditions under which that happens, what fixes it, and which questions remain open.

The scope images referenced below are in
[Scope measurements power supply](Scope%20easurements%20power%20supply).

---

## Recommendations

**1. Set `PowerWhenIdle = 1` for every servo output.** Do not switch the servo power
on and off around a movement. This is the single change that removes the resets.

Two traps that can silently undo this:

- The CV must be written **to EEPROM**. Changing the default in the source without
  writing it to EEPROM has no effect on a decoder that is already initialised.
- In `myServo.cpp`, `configPowerSignal()` hard-codes `idlePowerIsOff = true` for
  `ServoType` 1…4; only `ServoType 0` reads the `PowerWhenIdle` CV. `ServoType 1` is
  labelled *Uhlenbrock Standard-Servo / Weinert Mein Antrieb*. Setting that CV to the
  "obvious" value therefore disables the fix. The same pattern applies to the idle
  pulse level, which is hard-coded HIGH for those servo types.

**2. The 680 mA current limit (R112 = 10 kΩ) is tight.** Both the LPW5209AB5F and the
MT9700 use the same relation, `I ≈ 6.8 kΩ / R_SET`.

- *Three-output board (V2.2, 2025/07)*: the solder jumper pads on the bottom side
  accept a resistor in parallel with the 10 kΩ. 5.6 kΩ gives 1.89 A, 4.7 kΩ gives
  2.13 A. **Prefer 5.6 kΩ** — the MT9700 specifies a maximum programmable limit of 2 A
  and the LPW5209 is a 2.1 A switch, so 4.7 kΩ puts the setting at or above the
  device's own internal ceiling.
- *Six-output board*: the parallel modification is not possible. Replace
  R112/R122/R132 by 4.7 kΩ (1.45 A) or 3.9 kΩ (1.74 A).

**3. An external board between decoder and servo** — a series Schottky (1N5819) in the
+5 V line, followed by 470 µF and 100 nF, plus a bleeder resistor to discharge the
capacitor when the servo is off — reduces the disturbance by roughly a factor three and
also prevents the current limit from being triggered under heavy servo load.

---

## Measurement setup

| | |
|---|---|
| Supply | 15 V toroidal transformer, 50 Hz, bridge rectifier, approx. 19 V DC |
| Input network (Servo-2 and Servo-3) | R10 ∥ R11 = 5 Ω in series, then C11 = 470 µF |
| Step-down | TPS54331, 5.0 V, output capacitance 2 × 47 µF |
| Load switch | LPW5209AB5F or MT9700 (SOT23-5), R112 = 10 kΩ → 680 mA |
| Output capacitor per servo | 10 µF (Servo-2, Servo-3), 47 µF + 100 nF (Servo-6) |
| Processor | AVR32DA48 (Servo-3, Servo-6), AVR64DA28 (Servo-2), brown-out detection at 2.7 V |

Scope: Siglent, 10× probes, DC coupling. Current measurements use a 100 mΩ shunt in the
servo's **GND return**, probed single-ended against board ground.

---

## 1. The problem

On the Servo-3 board, with the servo power switched around each movement, the 5 V rail
collapses far enough to cross the 2.7 V brown-out threshold and reset the processor.
It happens with both switch types and it is intermittent.

| | max | min | vs. BOD 2.7 V |
|---|---|---|---|
| output 1, switch bypassed by solder jumper | 5.76 V | 4.60 V | far above |
| output 3, LPW5209 | 6.52 V | 2.72 V | just above |
| output 3, same command, another run | 5.47 V | 4.54 V | far above |
| output 2, MT9700 | 6.51 V | 2.31 V | well below |

With the switch bypassed the rail never misbehaves:

![No switch](Scope%20easurements%20power%20supply/2026-11-10-servo3-no-switch-clean.png)

With a switch fitted it collapses:

![LPW5209](Scope%20easurements%20power%20supply/2026-11-10-servo3-lpw5209-collapse.png)
![MT9700](Scope%20easurements%20power%20supply/2026-11-10-servo3-mt9700-collapse.png)

The same output, the same servo and the same command is sometimes clean, which shows
this is a threshold that is only just crossed rather than a permanent property:

![Clean run](Scope%20easurements%20power%20supply/2026-11-10-servo3-lpw5209-clean-run.png)

## 2. The 5 V rail is a symptom — the input rail collapses first

Probing V_IN of the TPS54331 shows the actual failure: the regulator stops because its
input falls below the 3.5 V UVLO threshold.

| | V_IN max | V_IN min | outcome |
|---|---|---|---|
| output 3, reset | 19.23 V | **2.80 V** | below UVLO, processor resets |
| output 3, no reset | 19.14 V | 11.42 V | survives |
| output 1 (no switch), vigorous movement | 19.29 V | 11.73 V | survives |

![VIN collapse](Scope%20easurements%20power%20supply/2026-11-10-servo3-vin-collapse-reset.png)
![VIN survives](Scope%20easurements%20power%20supply/2026-11-10-servo3-vin-dip-no-reset.png)

The 100 Hz ripple on the input confirms the 50 Hz transformer source:

![100 Hz ripple](Scope%20easurements%20power%20supply/2026-11-10-servo3-vin-100hz-ripple.png)

## 3. The supply itself has margin

Measured either side of the 5 Ω series resistance while a servo moves on output 1:

| | max | min |
|---|---|---|
| before the resistor | 19.71 V | 17.20 V |
| after the resistor | 19.32 V | 16.88 V |

![Before](Scope%20easurements%20power%20supply/2026-11-10-servo3-input-before-5ohm.png)
![After](Scope%20easurements%20power%20supply/2026-11-10-servo3-input-after-5ohm.png)

The difference is only 0.33–0.39 V, which at 5 Ω is roughly 70–80 mA of input current,
about 1.5 W. Even with the servo moving vigorously, V_IN never drops below 11.73 V and
the processor never resets:

![Vigorous](Scope%20easurements%20power%20supply/2026-11-10-servo3-vin-vigorous-no-switch.png)

The supply is therefore not undersized for this load.

## 4. The fix

With `PowerWhenIdle = 1`, so that the load switch stays permanently enabled, the same
output with the same servo is clean:

| | peak-to-peak | lowest |
|---|---|---|
| without external board | 1.07 V | 4.62 V |
| with external board | 0.29 V | 4.84 V |

![Fix, no external board](Scope%20easurements%20power%20supply/2026-11-10-servo3-fix-powerwhenidle-no-extboard.png)
![Fix, with external board](Scope%20easurements%20power%20supply/2026-11-10-servo3-fix-powerwhenidle-with-extboard.png)

A second, independent trigger remains: when the current limit actually engages — which
can be provoked by holding the servo by hand — the processor resets as well. Raising the
limit (recommendation 2) or fitting the external board (recommendation 3) avoids this.

## 5. Verification with the current firmware (17 November 2026)

Servo-3 board, MT9700, Weinert/Uhlenbrock servo, `PowerWhenIdle = 1`:

| probe point | max | min |
|---|---|---|
| servo rail, detail (1 ms/div) | 5.39 V | 4.68 V |
| servo rail, overview (10 ms/div) | 5.36 V | 4.70 V |
| processor 5 V | 5.32 V | 4.76 V |

![Servo rail detail](Scope%20easurements%20power%20supply/2026-11-17-servo3-servo-rail-detail.png)
![Servo rail overview](Scope%20easurements%20power%20supply/2026-11-17-servo3-servo-rail-overview.png)
![Processor rail](Scope%20easurements%20power%20supply/2026-11-17-servo3-processor-rail.png)

The disturbance is pulse-synchronous: one event per 20 ms servo frame, starting at the
**falling** edge of the control pulse, which is where the servo drives its motor. This
is normal behaviour. Note that the measured 5.39 V is close to the 5.5 V upper limit of
the AVR DA operating range, although that applies to the servo rail and not to the
processor supply.

## 6. Board comparison (20 November 2026)

Same servos, servo power switched around each movement, no external board.

| board | servo | max | min |
|---|---|---|---|
| Servo-3 | SG90 | 5.19 V | 4.85 V |
| Servo-3 | Uhlenbrock | 5.24 V | **below 4.15 V** (off screen) |
| Servo-6 | SG90 | 5.12 V | 4.90 V |
| Servo-6 | Uhlenbrock | 5.28 V | 4.73 V |
| Servo-2 | Uhlenbrock | 5.51 V | 4.44 V |

![Servo-3 SG90](Scope%20easurements%20power%20supply/2026-11-20-servo3-sg90-5v.png)
![Servo-3 Uhlenbrock](Scope%20easurements%20power%20supply/2026-11-20-servo3-uhlenbrock-5v.png)
![Servo-6 SG90](Scope%20easurements%20power%20supply/2026-11-20-servo6-sg90-5v.png)
![Servo-6 Uhlenbrock](Scope%20easurements%20power%20supply/2026-11-20-servo6-uhlenbrock-5v.png)
![Servo-2 Uhlenbrock](Scope%20easurements%20power%20supply/2026-11-20-servo2-uhlenbrock-5v.png)

Two observations.

**The Servo-6 board behaves much better**, and four things in its power section differ
from V2.2: R10/R11 removed, SS34 Schottky bridge instead of 1N4007, the step-down
compensation retuned (R12 22 k → 51 k, C16 3.3 nF → 1 nF), and 47 µF + 100 nF per servo
output instead of 10 µF. In addition the processor has its own 7805 fed from the
rectified input, which is why it cannot be reset by anything the servos do.

**The Servo-2 board also behaves well, and that is harder to explain.** Its power
section is component for component identical to V2.2 — R10 ∥ R11 = 5 Ω, SM4007PL
bridge, 470 µF, TPS54331 with R12 = 22 k and C16 = 3.3 nF — and its servo section uses
the same LPW5209 with a 10 kΩ ISET resistor and a 10 µF output capacitor. The remaining
differences are two servo outputs instead of three, an AVR64DA28 instead of an
AVR32DA48, and a different PCB layout. This is unresolved; the layout is the most likely
candidate.

## 7. Servo current (20 November 2026)

Measured over a 100 mΩ shunt in the servo's GND return, with the scope bandwidth limited
to 20 MHz. Without that limit the thin flying leads of the shunt produce narrow spikes
that are measurement artefacts, not current.

`Base` is the statistical low level and `Top` the statistical high level; `Max` and `Min`
are single samples and are not used here.

| servo | Base | Top | structure |
|---|---|---|---|
| **Uhlenbrock 81420** | ≈ 0 A | **≈ 0.58 A** | one continuous block for the whole movement |
| TowerPro | ≈ 0 A | ≈ 0.43 A | discrete bursts, long quiet intervals |
| MBTronic | ≈ 0 A | ≈ 0.41 A | discrete bursts |

![Uhlenbrock current](Scope%20easurements%20power%20supply/2026-11-20-current-uhlenbrock.png)
![TowerPro current](Scope%20easurements%20power%20supply/2026-11-20-current-towerpro.png)
![MBTronic current](Scope%20easurements%20power%20supply/2026-11-20-current-mbtronic.png)

This explains why the Uhlenbrock servo stands out. It is not a heavier servo — Uhlenbrock
specifies 13 Ncm, less than an SG90 — and its peak current is not higher than the others.
It differs in two ways: the level is roughly twice as high, and there are **no quiet
intervals**. With the other servos the decoupling capacitor can recharge between bursts;
with the Uhlenbrock it cannot, so the switch has to carry the full average continuously.

Against a 680 mA limit that leaves:

| servo | current | margin to 680 mA |
|---|---|---|
| Uhlenbrock | 0.58 A | 13 % |
| TowerPro | 0.43 A | 31 % |
| MBTronic | 0.41 A | 40 % |
| TZT | 0.32 A | 53 % |

Note that the Uhlenbrock capture is vertically clipped, so 0.58 A is a lower bound.

## 8. What is established and what is not

**Established:**

- Switching the load switch on and off around a movement causes the resets; keeping it
  permanently enabled removes them.
- The reset is a brown-out: the 5 V rail follows V_IN, which drops below the TPS54331's
  3.5 V UVLO.
- The firmware is not the cause. Output 1 runs the same firmware with the same servo and
  the same commands and is clean; the only difference is hardware.
- The supply is not undersized. Input current during a movement is roughly 80 mA.
- The Uhlenbrock servo draws about twice the average current of the others and, unlike
  them, draws it continuously.
- When the current limit engages, the processor resets as well.

**Not established:**

- *Why* the enable/disable transition triggers the collapse. Several mechanisms were
  considered — back-EMF from the servo, the current limit chopping, the converter's
  overvoltage protection, an input-filter instability, ripple below UVLO, a missing
  input capacitor at the switch — and each was ruled out by measurement.
- Why the Servo-2 board, with an identical power schematic, does not show the problem.
- Why the resets were observed mainly on movements in the `+` direction. The current
  measurements show no corresponding asymmetry.

For anyone picking this up: the useful starting point is that the current-limit trigger
**can be provoked on demand** by holding the servo by hand. That makes a fast, properly
grounded measurement possible — both channels on the pins of the switch itself, a few
millimetres of ground spring instead of a 10 cm ground lead, at 10–20 µs/div. No
measurement in this series was made that way.

---

## Measurement notes

- The schematic has **no capacitor on VIN of the load switch**, while the MT9700
  datasheet asks for 10 µF or more as close to the pin as possible, both in the pin table
  (page 2) and in the *Input capacitor* section (page 5). This is a deviation from the
  datasheet, although it does not explain the behaviour described above.
- The effect is intermittent. A single clean capture proves nothing; repeat at least ten
  times per direction.
- For current measurements, limit the scope bandwidth to 20 MHz and keep the measurement
  loop small. Twisting the shunt leads and using a ground spring helps more than the
  bandwidth limit: the limit suppresses the artefact, a small loop prevents it.
