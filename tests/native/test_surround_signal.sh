#!/usr/bin/env bash
# Verify actual binaural DSP, inside test_pipewire.sh's isolated PipeWire daemon.
set -euo pipefail
ICUE_SIGNAL_ARTIFACTS="${1:?Pass the test artifact directory}"
ICUE_SOURCE_PID=""
ICUE_RECORDER_PID=""
ICUE_SIGNAL_WAIT_ATTEMPTS=80
cleanup_signal() {
    if [[ -n "$ICUE_SOURCE_PID" ]]; then kill "$ICUE_SOURCE_PID" 2>/dev/null || true; wait "$ICUE_SOURCE_PID" 2>/dev/null || true; fi
    if [[ -n "$ICUE_RECORDER_PID" ]]; then kill "$ICUE_RECORDER_PID" 2>/dev/null || true; wait "$ICUE_RECORDER_PID" 2>/dev/null || true; fi
}
trap cleanup_signal EXIT
# A private daemon has no session manager. Configure the discovered adapter nodes
# explicitly, following PipeWire's minimal.conf PortConfig schema (also on 1.4).
ICUE_INPUT_NODE="$(jq -er '.[] | select(.type == "PipeWire:Interface:Node" and .info.props["node.name"] == "open-icue.input") | .id' "$ICUE_SIGNAL_ARTIFACTS/surround-nodes.json")"
ICUE_OUTPUT_NODE="$(jq -er '.[] | select(.type == "PipeWire:Interface:Node" and .info.props["node.name"] == "open-icue.output") | .id' "$ICUE_SIGNAL_ARTIFACTS/surround-nodes.json")"
pw-cli set-param "$ICUE_INPUT_NODE" PortConfig '{ direction = Input mode = dsp format = { mediaType = audio mediaSubtype = raw format = F32P rate = 48000 channels = 8 position = [ FL FR FC LFE RL RR SL SR ] } }' > "$ICUE_SIGNAL_ARTIFACTS/port-config.log" 2>&1
pw-cli set-param "$ICUE_OUTPUT_NODE" PortConfig '{ direction = Output mode = dsp format = { mediaType = audio mediaSubtype = raw format = F32P rate = 48000 channels = 2 position = [ FL FR ] } }' >> "$ICUE_SIGNAL_ARTIFACTS/port-config.log" 2>&1
python3 - "$ICUE_SIGNAL_ARTIFACTS" <<'PY'
import sys,struct,wave
from pathlib import Path
p=Path(sys.argv[1])
# Eight equal-energy impulses, one speaker at a time, with one second of leading silence.
rate=48000
frames=bytearray(rate*3*8*2)
for channel in range(8):
    struct.pack_into('<h',frames,((rate+channel*6000)*8+channel)*2,8192)
with wave.open(str(p/'surround-impulses.wav'),'wb') as w:
    w.setnchannels(8);w.setsampwidth(2);w.setframerate(rate);w.writeframes(frames)
PY
timeout 12 pw-cat --record --raw --rate 48000 --channels 2 --channel-map FL,FR --format f32 --target 0 --properties '{ node.name = "icue-test-recording" adapter.auto-port-config = { mode = dsp } }' - > "$ICUE_SIGNAL_ARTIFACTS/surround-capture.raw" 2> "$ICUE_SIGNAL_ARTIFACTS/record.log" &
ICUE_RECORDER_PID=$!
timeout 12 pw-cat --playback --channel-map FL,FR,FC,LFE,RL,RR,SL,SR --target 0 --properties '{ node.name = "icue-test-playback" adapter.auto-port-config = { mode = dsp } }' "$ICUE_SIGNAL_ARTIFACTS/surround-impulses.wav" > "$ICUE_SIGNAL_ARTIFACTS/play.log" 2>&1 &
ICUE_SOURCE_PID=$!
ICUE_PORTS_READY=false
for ((i=0;i<ICUE_SIGNAL_WAIT_ATTEMPTS;++i)); do
    pw-link -o > "$ICUE_SIGNAL_ARTIFACTS/output-ports.txt"
    pw-link -i > "$ICUE_SIGNAL_ARTIFACTS/input-ports.txt"
    if [[ "$(< "$ICUE_SIGNAL_ARTIFACTS/output-ports.txt")" == *"icue-test-playback:output_SR"* && "$(< "$ICUE_SIGNAL_ARTIFACTS/output-ports.txt")" == *"open-icue.output:output_FR"* && "$(< "$ICUE_SIGNAL_ARTIFACTS/input-ports.txt")" == *"icue-test-recording:input_FR"* && "$(< "$ICUE_SIGNAL_ARTIFACTS/input-ports.txt")" == *"open-icue.input:playback_SR"* ]]; then ICUE_PORTS_READY=true;break;fi
    sleep 0.05
done
if [[ "$ICUE_PORTS_READY" != true ]]; then
    pw-dump > "$ICUE_SIGNAL_ARTIFACTS/signal-nodes.json"
    cat "$ICUE_SIGNAL_ARTIFACTS/play.log" "$ICUE_SIGNAL_ARTIFACTS/record.log" >&2
    printf 'Signal source/recorder ports did not become ready\n' >&2
    exit 1
fi
pw-link open-icue.output:output_FL icue-test-recording:input_FL
pw-link open-icue.output:output_FR icue-test-recording:input_FR
for ICUE_CHANNEL in FL FR FC LFE RL RR SL SR; do
    pw-link "icue-test-playback:output_$ICUE_CHANNEL" "open-icue.input:playback_$ICUE_CHANNEL"
done
wait "$ICUE_SOURCE_PID"
ICUE_SOURCE_PID=""
sleep 0.15
kill "$ICUE_RECORDER_PID"
wait "$ICUE_RECORDER_PID" || true
ICUE_RECORDER_PID=""
python3 - "$ICUE_SIGNAL_ARTIFACTS" <<'PY'
import json,math,struct,sys
from pathlib import Path
p=Path(sys.argv[1]);b=(p/'surround-capture.raw').read_bytes()
values=struct.unpack('<'+'f'*(len(b)//4),b)
assert values and all(math.isfinite(x) for x in values),'Missing or nonfinite DSP output'
energies=[sum(x*x for x in values[ear::2]) for ear in (0,1)]
assert min(energies)>1e-8,'One binaural output is silent'
assert max(abs(x) for x in values)<1,'Unexpected clipping'
left=values[0::2];right=values[1::2]
diff=sum((l-r)**2 for l,r in zip(left,right))
assert diff>1e-8,'Surround output collapsed to dual mono'
# Distinct impulse bursts prove all eight inputs reached the convolver graph.
bursts=[];previous=-10000
for i,(l,r) in enumerate(zip(left,right)):
    if max(abs(l),abs(r))>1e-6:
        if i-previous>1000:bursts.append(i)
        previous=i
assert len(bursts)==8,f'Expected eight channel impulses, observed {len(bursts)}'
report={'isOk':True,'frames':len(left),'energies':energies,'differenceEnergy':diff,'inputImpulseCount':len(bursts),'peak':max(abs(x) for x in values)}
(p/'surround-signal.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
PY
