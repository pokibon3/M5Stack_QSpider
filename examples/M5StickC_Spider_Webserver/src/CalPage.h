/**************************************************************
   Calibration page for servo center position
 **************************************************************/

#ifndef CalPage_h
#define CalPage_h

const char CAL_Page[] = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Q-Spider Calibration</title>
<style>
body { text-align: center; font-family: "Trebuchet MS", Arial; margin: 0 auto; max-width: 480px; padding: 0 8px; }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(230px, 1fr)); gap: 8px; }
.leg { border: 1px solid #999; border-radius: 6px; padding: 4px; }
.leg h3 { margin: 4px 0; }
.row { margin: 6px 0; }
.row span.name { display: block; font-size: 13px; color: #555; }
.row b { display: inline-block; width: 40px; }
.row small { color: #777; }
button { margin: 2px; width: 40px; height: 36px; color: #fff; background-color: #2f4f4f; border-radius: 4px; border: none; }
button.wide { width: auto; padding: 0 12px; background-color: #4f2f2f; }
#msg { height: 1.4em; color: #060; }
</style>
</head><body>
<h2>Leg Center Calibration</h2>
<p>Home position: legs straight out to the side, knees as when lying down.</p>
<div class="grid" id="legs"></div>
<p id="msg"></p>
<button class="wide" onclick="api('home=1')">Home</button>
<button class="wide" onclick="api('save=1','Saved')">Save</button>
<button class="wide" onclick="api('load=1','Loaded saved values')">Load</button>
<button class="wide" onclick="api('reset=1','Defaults (not saved)')">Default</button>
<p><a href="/">Back to controller</a></p>
<script>
// [label, knee ch, leg ch] in top view (front is up)
const LEGS = [['Front Left', 2, 3], ['Front Right', 0, 1], ['Rear Left', 6, 7], ['Rear Right', 4, 5]];
function row(ch, name) {
  return `<div class="row"><span class="name">${name}</span>` +
    `<button onclick="adj(${ch},-5)">-5</button><button onclick="adj(${ch},-1)">-1</button>` +
    `<b id="v${ch}">-</b>` +
    `<button onclick="adj(${ch},1)">+1</button><button onclick="adj(${ch},5)">+5</button>` +
    `<br><small id="d${ch}"></small></div>`;
}
document.getElementById('legs').innerHTML = LEGS.map(l =>
  `<div class="leg"><h3>${l[0]}</h3>${row(l[1], 'Knee (+ = up)')}${row(l[2], 'Leg (+ = forward)')}</div>`).join('');

function show(j) {
  // show trim from default in physical direction (+ = up / forward)
  j.cent.forEach((v, i) => {
    const t = (v - j.def[i]) * j.dir[i];
    document.getElementById('v' + i).textContent = (t > 0 ? '+' : '') + t;
    document.getElementById('d' + i).textContent = `servo ${v}°`;
  });
}
function api(q, msg) {
  fetch('/api/cal?' + q).then(r => r.json()).then(j => {
    show(j);
    document.getElementById('msg').textContent = msg || (j.dirty ? 'Not saved' : '');
  });
}
function adj(ch, d) { api(`adj=${ch}&d=${d}`); }
api('home=1');
</script>
</body></html>
)rawliteral";

#endif
