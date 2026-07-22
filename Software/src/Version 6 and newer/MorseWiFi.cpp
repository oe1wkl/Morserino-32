/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                                 ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

#include "morsedefs.h"
#include "MorseWiFi.h"
#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseJSON.h"

#ifdef CONFIG_PRACTICE_STATS
#include "MorsePracticeStats.h"
#endif

////////////////// Variables for file handling and WiFi functions

// File file;

WebServer MorseWiFi::server(80);    // Create a webserver object that listens for HTTP request on port 80
//WiFiUDP   MorseWiFi::udp;           // Create udp socket for wifi tx
AsyncUDP MorseWiFi::audp;           // Create async udp socket for wifi rx

File MorseWiFi::fsUploadFile;              // a File object to temporarily store the received file

const char* MorseWiFi::host = "m32";               // hostname of the webserver


/// WiFi constants
const char* MorseWiFi::ssid = "morserino";
const char* MorseWiFi::password = "";


// HTML for the AP server - used to get SSID and Password for local WiFi network - needed for file upload and OTA SW updates
const char* MorseWiFi::myForm = "<html><head><meta charset='utf-8'><title>Get AP Info</title><style> form {width: 420px;}div { margin-bottom: 20px;}"
                "label {display: inline-block; width: 240px; text-align: right; padding-right: 10px;} button, input {float: right;}</style>"
                "</head>"
                "<body>"
                  "<form action='/set' method='get'>"
                    "<div>"   // fields for network 1
                      "<div>"
                        "<label for='ssid1'>SSID of WiFi network 1?</label>"
                        "<input name='ssid1' id='ssid1' value='SSIDV1'>"
                      "</div>"
                      "<div>"
                        "<label for='pw1'>WiFi Password 1?</label>"
                        "<input name='pw1' id='pw1'>"
                      "</div>"
                      "<div>"
                        "<label for='trxpeer1'>WiFi TRX Peer IP/Host 1?</label>"
                        "<input name='trxpeer1' id='trxpeer1' value='PEERIPV1'>"
                      "</div>"
                    "</div>"

                    "<div>"   // fields for network 2
                      "<div>"
                        "<label for='ssid2'>SSID of WiFi network 2?</label>"
                        "<input name='ssid2' id='ssid2' value='SSIDV2'>"
                      "</div>"
                      "<div>"
                        "<label for='pw2'>WiFi Password 2?</label>"
                        "<input name='pw2' id='pw2'>"
                      "</div>"
                      "<div>"
                        "<label for='trxpeer2'>WiFi TRX Peer IP/Host 2?</label>"
                        "<input name='trxpeer2' id='trxpeer2' value='PEERIPV2'>"
                      "</div>"
                    "</div>"

                    "<div>"   // fields for network 3
                      "<div>"
                        "<label for='ssid3'>SSID of WiFi network 3?</label>"
                        "<input name='ssid3' id='ssid3' value='SSIDV3'>"
                      "</div>"
                      "<div>"
                        "<label for='pw3'>WiFi Password 3?</label>"
                        "<input name='pw3' id='pw3'>"
                      "</div>"
                      "<div>"
                        "<label for='trxpeer3'>WiFi TRX Peer IP/Host 3?</label>"
                        "<input name='trxpeer3' id='trxpeer3' value='PEERIPV3'>"
                      "</div>"
                    "</div>"

                    "<div>"
                      "(255.255.255.255 = Local Broadcast IP will be used as Peer if empty)"
                    "</div>"
                    "<div>"
                      "<button>Submit</button>"
                    "</div>"
                  "</form>"
                "</body>"
              "</html>";


/*
 * HTML for Upload Login page
 */

const char* MorseWiFi::uploadLoginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>M32 File Upload - Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='m32' && form.pwd.value=='upload')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";


const char* MorseWiFi::updateLoginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>M32 Firmware Update Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='m32' && form.pwd.value=='update')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";


const char* MorseWiFi::serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Begin'>"
    "</form>"
 "<div id='prg'>Progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

#ifdef CONFIG_PRACTICE_STATS
// Practice Stats page: self-contained (no CDN — the device's own AP has no
// internet), fetches /api/stats.jsonl and aggregates client-side. Posts the
// browser's clock to /api/time once on load; see MorsePracticeStats.h and
// devdocs/practice-stats/README.md for the log format and the reasoning
// behind syncing the clock this way instead of NTP.
const char* MorseWiFi::statsPage = R"statspage(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>M32 Practice Stats</title>
<style>
body{font-family:sans-serif;max-width:640px;margin:0 auto;padding:12px;background:#f4f4f4;color:#222;overflow-x:hidden}
h1{font-size:1.2em} h2{font-size:1em;margin-top:1.6em;border-bottom:1px solid #ccc;padding-bottom:.2em}
.row{display:flex;align-items:center;margin:.3em 0;font-size:.9em}
.label{width:5.5em;flex-shrink:0}
.bar{flex-grow:1;background:#ddd;border-radius:3px;height:1.1em;margin:0 .5em;overflow:hidden;display:flex}
.bar .listen{background:#3a8a4a;height:100%}
.bar .send{background:#2d6cb0;height:100%}
table{width:100%;border-collapse:collapse;font-size:.85em;white-space:nowrap}
td,th{text-align:left;padding:.25em .5em;border-bottom:1px solid #ddd}
.scroll{overflow-x:auto;max-width:100%}
.scrollhint{font-size:.8em;color:#888;font-style:italic;margin:.3em 0 0}
button{padding:.5em 1em;margin-top:.5em}
#storage{font-size:.85em;color:#555}
.tag{display:inline-block;padding:.05em .5em;border-radius:3px;font-size:.85em;color:#fff}
.tag.listen{background:#3a8a4a}
.tag.send{background:#2d6cb0}
.legend{font-size:.8em;color:#555;margin-top:.3em}
.legend .tag{margin-right:.5em}
.charlegend{font-size:.8em;color:#555;margin:.3em 0 .8em;display:flex;flex-wrap:wrap;gap:.9em}
.charlegend span{display:inline-flex;align-items:center;gap:.35em}
.swatch{width:.8em;height:.8em;border-radius:2px;display:inline-block}
.summary{display:flex;flex-wrap:wrap;gap:1.5em;font-size:.85em;margin:.3em 0 .8em}
.summary b{display:block;font-size:1.2em}
.needschip{display:inline-block;background:#f4dcd4;color:#a8402f;font-weight:bold;font-family:monospace;
  border-radius:3px;padding:.1em .45em;margin:.25em .3em 0 0;font-size:.95em}
.tilegrid{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:.8em}
.tile{position:relative;width:2.1em;height:2.1em;border-radius:4px;display:flex;align-items:center;justify-content:center;
  font-family:monospace;font-size:1em;font-weight:bold;cursor:pointer}
.tile.good{background:#dcebe0;color:#2f6b46}
.tile.mid{background:#f3e6cc;color:#8a5f14}
.tile.weak{background:#f4dcd4;color:#a8402f}
.tile.unseen{background:#e6e6e6;color:#888}
.tile.future{background:#f4f4f4;color:#ccc;border:1px dashed #ddd;cursor:default}
.tile.selected{outline:2px solid #2d6cb0}
.tile .tip{position:absolute;bottom:120%;left:50%;transform:translateX(-50%);background:#222;color:#fff;
  font-family:sans-serif;font-size:.72em;padding:.3em .5em;border-radius:4px;white-space:nowrap;
  opacity:0;pointer-events:none;transition:opacity .1s}
.tile:hover .tip{opacity:1}
#chars th{cursor:pointer;user-select:none}
#chars th.sorted::after{content:" \25be"}
</style></head><body>
<h1>Practice Stats</h1>
<div id="storage">loading&hellip;</div>

<h2>Time per Koch Lesson</h2>
<div class="legend"><span class="tag listen">Listen</span><span class="tag send">Send</span> &mdash; <span id="totalTime"></span></div>
<div id="lessons"></div>

<h2>Characters</h2>
<div class="charlegend">
<span><span class="swatch" style="background:#dcebe0;border:1px solid #2f6b46"></span>Solid (&lt;10% err)</span>
<span><span class="swatch" style="background:#f3e6cc;border:1px solid #8a5f14"></span>Shaky (10&ndash;25%)</span>
<span><span class="swatch" style="background:#f4dcd4;border:1px solid #a8402f"></span>Weak (&gt;25%)</span>
<span><span class="swatch" style="background:#e6e6e6;border:1px solid #888"></span>Heard, not sent</span>
<span><span class="swatch" style="background:#f4f4f4;border:1px dashed #ccc"></span>Not yet learned</span>
</div>
<div class="summary" id="charSummary"></div>
<div class="tilegrid" id="charGrid">loading&hellip;</div>
<div class="scroll"><table id="chars"><thead><tr>
<th data-key="c">Char</th><th data-key="heard">Heard</th><th data-key="attempts">Sent</th>
<th data-key="correct">Correct</th><th data-key="errors">Errors</th><th data-key="rate" class="sorted">Rate</th>
</tr></thead><tbody></tbody></table></div>

<h2>Session History</h2>
<p class="scrollhint">Scroll sideways to see all columns &rarr;</p>
<div class="scroll"><table id="sessions"><thead><tr>
<th>When</th><th>Lesson</th><th>Type</th><th>Words</th><th>Chr/Wd</th>
<th>WPM</th><th>ICS</th><th>IWS</th><th>WPS</th><th>CPS</th>
<th>Errors</th><th>Err%</th><th>Duration</th>
</tr></thead><tbody></tbody></table></div>

<button onclick="clearLog()">Clear Log</button>

<script>
function fmtDur(s){
  if(s<60) return s+"s";
  var m=Math.floor(s/60), h=Math.floor(m/60);
  m=m%60;
  return h>0 ? (h+"h "+m+"m") : (m+"m "+(s%60)+"s");
}
function escHtml(s){
  return String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;");
}
fetch("/api/time",{method:"POST",headers:{"Content-Type":"application/json"},
  body:JSON.stringify({epoch:Math.floor(Date.now()/1000)})});

fetch("/api/storage").then(r=>r.json()).then(function(d){
  var pct=Math.round(100*d.used/d.total);
  document.getElementById("storage").textContent =
    "Storage: "+Math.round(d.used/1024)+" KB / "+Math.round(d.total/1024)+" KB used ("+pct+"%)";
});

Promise.all([
  fetch("/api/koch").then(r=>r.json()).catch(()=>null),
  fetch("/api/stats.jsonl").then(r=>r.text())
]).then(function(results){
  var koch=results[0], text=results[1];
  var records=[];
  text.split("\n").forEach(function(line){
    line=line.trim();
    if(!line) return;
    try { records.push(JSON.parse(line)); } catch(e) { /* skip malformed line */ }
  });

  var lessonTime={}, charStats={};
  records.forEach(function(r){
    if(!lessonTime[r.lesson]) lessonTime[r.lesson]={listen:0,send:0};
    lessonTime[r.lesson][r.mode==="send"?"send":"listen"]+=r.dur;
    if(r.chars) Object.keys(r.chars).forEach(function(c){
      var h=r.chars[c][0], a=r.chars[c][1], e=r.chars[c][2];
      if(!charStats[c]) charStats[c]={heard:0,attempts:0,errors:0};
      charStats[c].heard+=h; charStats[c].attempts+=a; charStats[c].errors+=e;
    });
  });

  var lessons=Object.keys(lessonTime).map(Number).sort(function(a,b){return a-b;});
  var maxTime=Math.max.apply(null,lessons.map(function(l){return lessonTime[l].listen+lessonTime[l].send;}).concat([1]));
  var grandTotal=records.reduce(function(sum,r){return sum+r.dur;},0);
  document.getElementById("totalTime").textContent="Total: "+fmtDur(grandTotal)+" across "+records.length+" session"+(records.length===1?"":"s");
  var lessonsDiv=document.getElementById("lessons");
  if(lessons.length===0) lessonsDiv.textContent="No practice logged yet.";
  lessons.forEach(function(l){
    var lt=lessonTime[l], total=lt.listen+lt.send;
    var listenPct=Math.round(100*lt.listen/maxTime), sendPct=Math.round(100*lt.send/maxTime);
    var row=document.createElement("div"); row.className="row";
    row.innerHTML='<span class="label">Lesson '+l+'</span><span class="bar">'+
      '<div class="listen" style="width:'+listenPct+'%" title="Listen: '+fmtDur(lt.listen)+'"></div>'+
      '<div class="send" style="width:'+sendPct+'%" title="Send: '+fmtDur(lt.send)+'"></div>'+
      '</span><span>'+fmtDur(total)+"</span>";
    lessonsDiv.appendChild(row);
  });

  function bucket(cs){
    if(!cs || cs.attempts===0) return "unseen";
    var rate=cs.errors/cs.attempts;
    if(rate<0.10) return "good";
    if(rate<=0.25) return "mid";
    return "weak";
  }

  var gridDiv=document.getElementById("charGrid");
  gridDiv.innerHTML="";
  var rows=[];
  if(koch && koch.characters){
    koch.characters.forEach(function(ch,i){
      var pos=koch.minimum+i;
      var tile=document.createElement("span");
      if(pos>koch.value){
        tile.className="tile future";
        tile.textContent=ch;
        gridDiv.appendChild(tile);
        return;
      }
      var cs=charStats[ch]||{heard:0,attempts:0,errors:0};
      var cls=bucket(cs);
      tile.className="tile "+cls;
      tile.dataset.char=ch;
      var rate=cs.attempts?Math.round(100*cs.errors/cs.attempts):null;
      tile.innerHTML="<span>"+escHtml(ch===" "?"␣":ch)+"</span>"+
        "<span class=\"tip\">heard "+cs.heard+" &middot; sent "+cs.attempts+
        (cs.attempts?" &middot; "+rate+"% err":" &middot; not sent yet")+"</span>";
      gridDiv.appendChild(tile);
      if(cs.heard>0 || cs.attempts>0) rows.push({c:ch,heard:cs.heard,attempts:cs.attempts,correct:cs.attempts-cs.errors,errors:cs.errors,rate:cs.attempts?rate:-1});
    });
  } else {
    // /api/koch unavailable — fall back to whatever characters actually showed up in the log
    gridDiv.textContent="Lesson sequence unavailable — see the table below.";
    Object.keys(charStats).forEach(function(ch){
      var cs=charStats[ch];
      rows.push({c:ch,heard:cs.heard,attempts:cs.attempts,correct:cs.attempts-cs.errors,errors:cs.errors,rate:cs.attempts?Math.round(100*cs.errors/cs.attempts):-1});
    });
  }
  if(koch && koch.characters && rows.length===0) gridDiv.textContent="No practice logged yet.";

  var learnedCount = koch ? Math.min(koch.value,koch.characters.length) : rows.length;
  var totalAttempts=0, totalErrors=0;
  var sent=rows.filter(function(r){ return r.attempts>0; });
  sent.forEach(function(r){ totalAttempts+=r.attempts; totalErrors+=r.errors; });
  var top5=sent.slice().sort(function(a,b){
    return b.rate-a.rate || b.attempts-a.attempts;
  }).slice(0,5);
  var summaryDiv=document.getElementById("charSummary");
  summaryDiv.innerHTML =
    "<div>Learned<br><b>"+learnedCount+(koch?" / "+koch.characters.length:"")+"</b></div>"+
    "<div>Accuracy (sent)<br><b>"+(totalAttempts?Math.round(100*(totalAttempts-totalErrors)/totalAttempts)+"%":"&mdash;")+"</b></div>"+
    "<div>Needs work<br>"+(top5.length?top5.map(function(r){return "<span class=\"needschip\">"+escHtml(r.c)+" "+r.rate+"%</span>";}).join(""):"<b>&mdash;</b>")+"</div>";

  var charsBody=document.querySelector("#chars tbody");
  var charSortKey="rate", charSortDir=-1;
  function renderCharRows(){
    var sorted=rows.slice().sort(function(a,b){
      var av=a[charSortKey], bv=b[charSortKey];
      if(typeof av==="string") return charSortDir*av.localeCompare(bv);
      return charSortDir*(av-bv);
    });
    charsBody.innerHTML="";
    sorted.forEach(function(r){
      var cls=r.attempts?bucket({attempts:r.attempts,errors:r.errors}):"unseen";
      var tr=document.createElement("tr");
      tr.dataset.char=r.c;
      tr.innerHTML="<td>"+escHtml(r.c)+"</td><td>"+r.heard+"</td><td>"+r.attempts+"</td><td>"+r.correct+"</td><td>"+r.errors+
        "</td><td class=\""+cls+"\">"+(r.attempts?r.rate+"%":"&mdash;")+"</td>";
      charsBody.appendChild(tr);
    });
  }
  renderCharRows();
  document.querySelectorAll("#chars thead th").forEach(function(th){
    th.addEventListener("click",function(){
      var key=th.dataset.key;
      if(charSortKey===key) charSortDir*=-1; else { charSortKey=key; charSortDir=(key==="c")?1:-1; }
      document.querySelectorAll("#chars thead th").forEach(function(t){t.classList.remove("sorted");});
      th.classList.add("sorted");
      renderCharRows();
    });
  });
  gridDiv.addEventListener("click",function(e){
    var tile=e.target.closest(".tile");
    if(!tile || !tile.dataset.char) return;
    document.querySelectorAll(".tile.selected").forEach(function(t){t.classList.remove("selected");});
    tile.classList.add("selected");
    var row=document.querySelector("#chars tbody tr[data-char=\""+CSS.escape(tile.dataset.char)+"\"]");
    if(row) row.scrollIntoView({behavior:"smooth",block:"center"});
  });

  records.sort(function(a,b){return b.ts-a.ts;});   // newest first (undated [ts=0] sink to the bottom)
  var sessBody=document.querySelector("#sessions tbody");
  var dash="&mdash;";
  records.forEach(function(r){
    var when = r.ts>0 ? new Date(r.ts*1000).toLocaleString() : "(undated)";
    var isSend = r.mode==="send";
    var words = r.words||0;
    // tc (from wordPresented(), both modes) = actual content length — the right
    // basis for Chr/Wd and CPS. attempts/errors (from r.chars, send only) can
    // exceed tc when a word gets repeated after a wrong answer, so they're only
    // used for the error rate, not as a stand-in for word length.
    var tc = r.tc||0;
    var attempts=0, totalErrors=0;
    if(r.chars) Object.keys(r.chars).forEach(function(c){ attempts+=r.chars[c][1]; totalErrors+=r.chars[c][2]; });
    var charsPerWord = words>0 ? (tc/words).toFixed(1) : dash;
    var wps = r.dur>0 ? (words/r.dur).toFixed(2) : dash;
    var cps = r.dur>0 ? (tc/r.dur).toFixed(2) : dash;
    var errRate = isSend && attempts>0 ? Math.round(100*totalErrors/attempts)+"%" : dash;
    var tr=document.createElement("tr");
    tr.innerHTML=
      "<td>"+when+"</td>"+
      "<td>"+r.lesson+"</td>"+
      "<td><span class=\"tag "+(isSend?"send":"listen")+"\">"+(isSend?"Send":"Listen")+"</span></td>"+
      "<td>"+words+"</td>"+
      "<td>"+charsPerWord+"</td>"+
      "<td>"+(r.wpm||dash)+"</td>"+
      "<td>"+(r.ics!==undefined?r.ics:dash)+"</td>"+
      "<td>"+(r.iws!==undefined?r.iws:dash)+"</td>"+
      "<td>"+wps+"</td>"+
      "<td>"+cps+"</td>"+
      "<td>"+(isSend?totalErrors:dash)+"</td>"+
      "<td>"+errRate+"</td>"+
      "<td>"+fmtDur(r.dur)+"</td>";
    sessBody.appendChild(tr);
  });
});

function clearLog(){
  if(!confirm("Clear all practice stats? This cannot be undone.")) return;
  fetch("/api/stats/clear",{method:"POST"}).then(function(){ location.reload(); });
}
</script>
</body></html>
)statspage";
#endif

namespace internal
{
    String getContentType(String filename); // convert the file extension to the MIME type
    boolean handleFileRead(String path);       // send the right file to the client (if it exists)
    void handleFileUpload();                // upload a new file to the SPIFFS
    void handleNotFound();
    void startMDNS();
    boolean errorConnect(String msg);
}



void internal::handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += MorseWiFi::server.uri();
  message += "\nMethod: ";
  message += (MorseWiFi::server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += MorseWiFi::server.args();
  message += "\n";
  for (uint8_t i = 0; i < MorseWiFi::server.args(); i++) {
    message += " " + MorseWiFi::server.argName(i) + ": " + MorseWiFi::server.arg(i) + "\n";
  }
  MorseWiFi::server.send(404, "text/plain", message);
}

void MorseWiFi::menuNetSelect() {
  const int numNetworks = 4;
  String names[numNetworks]; String peers[numNetworks];
  names[0] = "0: EspNow";
  peers[0] = "Broadcast";
  names[1] = "1: " + MorsePreferences::wlanSSID1;
  peers[1] = "/ " + MorsePreferences::wlanTRXPeer1;
  names[2] = "2: " + MorsePreferences::wlanSSID2;
  peers[2] = "/ " + MorsePreferences::wlanTRXPeer2;
  names[3] = "3: " + MorsePreferences::wlanSSID3;
  peers[3] = "/ " + MorsePreferences::wlanTRXPeer3;

  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0,  "Select Wifi");

  int btnClicks;
  int8_t choice = MorsePreferences::wlanChoice;  // start with last used network
  int8_t previousChoice = -1;
  while (true) {
    checkShutDown(false);  // possibly time-out: go to sleep
    if(choice!=previousChoice) {
      MorseOutput::clearScrollLines();
      MorseOutput::printOnScroll(0, REGULAR, 0, names[choice] );
      MorseOutput::printOnScroll(1, REGULAR, 0, peers[choice] );
      if (m32protocol) {
                  MorseJSON::jsonCreate("message", names[choice], "");
                  MorseJSON::jsonCreate("message", peers[choice], "");
      }
      previousChoice = choice;
    }
    switch(checkEncoder()){
      case -1:
        if(choice == 0) {
          choice = numNetworks;
        }
        choice--;
        break;
      case 1:
        choice++;
        if(choice >= numNetworks){
          choice = 0;
        }
        break;
    }
    Buttons::modeButton.Update();
    btnClicks = Buttons::modeButton.clicks;
    if(btnClicks == 1)
      break;
    if(btnClicks == -1){
      choice = -1;
      break;
    }
  }

  switch(choice) {
    case 0: MorsePreferences::useEspNow = true;
      break;
    case 1: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID1, MorsePreferences::wlanPassword1, MorsePreferences::wlanTRXPeer1);
            MorsePreferences::useEspNow = false;
      break;
    case 2: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID2, MorsePreferences::wlanPassword2, MorsePreferences::wlanTRXPeer2);
            MorsePreferences::useEspNow = false;
      break;
    case 3: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID3, MorsePreferences::wlanPassword3, MorsePreferences::wlanTRXPeer3);
            MorsePreferences::useEspNow = false;
      break;
    case -1: // long (?) click pressed to exit menu
      return;
      break;
  }
  MorsePreferences::wlanChoice = choice;
  MorsePreferences::writePreferences("morserino");
  // No restart needed — menu_() will clean up WiFi state on return
}

void MorseWiFi::menuExec(uint8_t command) {
  String msg; msg.reserve(120);
  if (!MorsePreferences::useEspNow) {
      switch (command) {
        case  _wifi_mac:
                      WiFi.mode(WIFI_MODE_STA);               // init WiFi as client
                      MorseOutput::clearDisplay();
                      msg = "MAC Address is " + WiFi.macAddress();
                      MorseOutput::printOnStatusLine( true, 0,  WiFi.macAddress());

                      delay(1000);
                      if (m32protocol) {
                        MorseJSON::jsonCreate("message", msg, "");
                        WiFi.disconnect(true,false);
                        return;
                      }
                      else {
                        delay(2000);
                        MorseOutput::printOnScroll(0, REGULAR, 0, "FN: return" );
                        while (true) {  // wait
                              checkShutDown(false);  // possibly time-out: go to sleep
                              if (digitalRead(volButtonPin) == LOW) {
                                WiFi.disconnect(true,false);
                                return;
                            }
                        } // end wait
                      }
                      break;
            case  _wifi_config:
                      MorseWiFi::startAP();          // run as AP to get WiFi credentials from user
                      break;
            case _wifi_check:
                      MorseOutput::clearDisplay();
                      msg = "Connecting... ";
                      MorseOutput::printOnStatusLine( true, 0,  msg);
                      if (m32protocol)
                        MorseJSON::jsonCreate("message", msg + "Please wait", "");
                      if (! MorseWiFi::wifiConnect())
                          msg = ""; //return false;
                      else {
                          msg = "Connected! " + MorsePreferences::wlanSSID + " - " + WiFi.localIP().toString();
                          MorseOutput::printOnStatusLine( true, 0,  "Connected!    ");
                          MorseOutput::printOnScroll(0, REGULAR, 0, MorsePreferences::wlanSSID);
                          MorseOutput::printOnScroll(1, REGULAR, 0, WiFi.localIP().toString(), true);
                      }
                      //WiFi.mode( WIFI_MODE_NULL ); // switch off WiFi
                      delay(1000);
                      if (m32protocol) {
                          if (msg != "")
                              MorseJSON::jsonCreate("message", msg, "");
                          WiFi.disconnect(true,false);
                          return;
                      }
                      else {
                        delay(1000);
                        MorseOutput::printOnScroll(2, REGULAR, 0, "FN: return" );
                        while (true) {
                              checkShutDown(false);  // possibly time-out: go to sleep
                              if (digitalRead(volButtonPin) == LOW) {
                                WiFi.disconnect(true,false);
                                MorseOutput::clearDisplay();
                                return;
                              }
                        }
                      }
                      break;
            case _wifi_upload:
                      MorseWiFi::uploadFile();       // upload a text file
                      break;
            case _wifi_update:
                      MorseWiFi::updateFirmware();   // run OTA update
                      break;
#ifdef CONFIG_PRACTICE_STATS
            case _wifi_stats:
                      MorseWiFi::viewStats();        // serve the Practice Stats web page
                      break;
#endif
      }
    } else {
      MorseOutput::clearDisplay();
      MorseOutput::printOnScroll(0, REGULAR, 0, "Not Available" ); 
      MorseOutput::printOnScroll(1, REGULAR, 0, "Using EspNow" ); 
      if (m32protocol)
          MorseJSON::jsonCreate("message", "Not available when using ESPNOW", "");
      delay(1800);                   
    }
}

static void shutdownWiFi() {
    MorseWiFi::server.stop();
    MorseWiFi::server.close();
    delay(100);
    WiFi.disconnect(true, false);
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
}

void MorseWiFi::startAP() {
  volatile bool configDone = false;   // flag to exit the server loop
 
  WiFi.mode(WIFI_AP);
  WiFi.setHostname(ssid);
  WiFi.softAP(ssid);
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0,    "Enter Wifi Info @");
  MorseOutput::printOnScroll(0, REGULAR, 0,  "AP: morserino");
  MorseOutput::printOnScroll(1, REGULAR, 0,  "URL: m32.local");
  MorseOutput::printOnScroll(2, REGULAR, 0,  "FN to abort");
  delay(200);
  internal::startMDNS();
 
  server.on("/", HTTP_GET, []() {
    String formular;
    formular.reserve(1024);
    formular = myForm;
    formular.replace("SSIDV1", MorsePreferences::wlanSSID1);
    formular.replace("PEERIPV1", MorsePreferences::wlanTRXPeer1);
    formular.replace("SSIDV2", MorsePreferences::wlanSSID2);
    formular.replace("PEERIPV2", MorsePreferences::wlanTRXPeer2);
    formular.replace("SSIDV3", MorsePreferences::wlanSSID3);
    formular.replace("PEERIPV3", MorsePreferences::wlanTRXPeer3);
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", formular);
  });
 
  server.on("/set", HTTP_GET, [&configDone]() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html",
        "Wifi Info updated. You can close this page.");
 
    MorsePreferences::writeWifiInfoMultiple(
      String(server.arg("ssid1")), String(server.arg("pw1")), String(server.arg("trxpeer1")),
      String(server.arg("ssid2")), String(server.arg("pw2")), String(server.arg("trxpeer2")),
      String(server.arg("ssid3")), String(server.arg("pw3")), String(server.arg("trxpeer3"))
    );
    configDone = true;    // signal the loop to exit
  });
 
  server.onNotFound(internal::handleNotFound);
 
  server.begin();
  while (!configDone) {
      server.handleClient();
      delay(20);
      Buttons::volButton.Update();
      if (Buttons::volButton.clicks) {
          break;                      // user pressed button to abort
      }
      checkShutDown(false);
  }
 
  // Clean shutdown — no restart needed
  shutdownWiFi();
 
  // Show brief confirmation
  MorseOutput::clearDisplay();
  if (configDone) {
      MorseOutput::printOnScroll(1, BOLD, 0, "WiFi Updated");
  } else {
      MorseOutput::printOnScroll(1, BOLD, 0, "Cancelled");
  }
  delay(1000);
  // Function returns to menuExec() → menu_() which reinitialises display
}
 

void MorseWiFi::updateFirmware()   {                   /// start wifi client, web server and upload new binary from a local computer
  if (! wifiConnect())
    return;

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updateLoginIndex);
  });

  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        DEBUG("Update Success: Rebooting...\n");
      } else {
        Update.printError(Serial);
      }
    }
  });
  //DEBUG("Starting web server");
  server.begin();
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Waiting f. Update ");
  MorseOutput::printOnScroll(0, REGULAR, 0,  "URL: m32.local");
  MorseOutput::printOnScroll(1, REGULAR, 0,  "IP:");
  MorseOutput::printOnScroll(2, REGULAR, 0, WiFi.localIP().toString(), true);
  unsigned long wifiTimeout = millis() + 300000UL;  // 5 minute timeout
  while(true) {
       server.handleClient();
       delay(10);
       Buttons::volButton.Update();
       if (Buttons::volButton.clicks) {
           break;
       }
       if (millis() > wifiTimeout) {
           MorseOutput::printOnScroll(1, BOLD, 0, "Timeout!");
           delay(1000);
           break;
       }
   }
   WiFi.disconnect(true, false);
   WiFi.mode(WIFI_OFF);
}


#ifdef CONFIG_PRACTICE_STATS
void MorseWiFi::viewStats() {                          /// start wifi client, web server, serve the Practice Stats page
  if (! wifiConnect())
    return;

  MorsePracticeStats::tryNtpSync();   // best-effort; browser sync (below) covers the rest

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "no-store");   // the page changes across firmware updates; never let the browser serve a stale copy
    server.send(200, "text/html", statsPage);
  });

  server.on("/api/time", HTTP_POST, []() {
    StaticJsonDocument<64> doc;
    deserializeJson(doc, server.arg("plain"));
    time_t epoch = doc["epoch"] | 0;
    MorsePracticeStats::setWallClock(epoch);
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/storage", HTTP_GET, []() {
    StaticJsonDocument<64> doc;
    doc["used"] = MorsePracticeStats::usedBytes();
    doc["total"] = MorsePracticeStats::totalBytes();
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // Koch character sequence, for the stats page's tile grid: which characters
  // are learned (index < value) vs. not yet (index >= value). Same data as
  // the serial protocol's "get kochlesson" (MorseJSON::jsonGetKoch()), which
  // writes straight to Serial and so isn't reusable here — rebuilt directly
  // from the same source (Koch::getKochChar()) for the HTTP path.
  server.on("/api/koch", HTTP_GET, []() {
    DynamicJsonDocument doc(2048);
    doc["value"] = MorsePreferences::kochFilter;
    doc["minimum"] = MorsePreferences::kochMinimum;
    doc["maximum"] = MorsePreferences::kochMaximum;
    JsonArray characters = doc.createNestedArray("characters");
    for (int i = MorsePreferences::kochMinimum - 1; i < MorsePreferences::kochMaximum; ++i) {
      String c = koch.getKochChar(i);
      characters.add(cleanUpProSigns(c));
    }
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/stats.jsonl", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    if (!SPIFFS.exists(MorsePracticeStats::logPath)) {
      server.send(200, "text/plain", "");
      return;
    }
    File f = SPIFFS.open(MorsePracticeStats::logPath, "r");
    server.streamFile(f, "text/plain");
    f.close();
  });

  server.on("/api/stats/clear", HTTP_POST, []() {
    MorsePracticeStats::clearLog();
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound(internal::handleNotFound);

  server.begin();
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Practice Stats");
  MorseOutput::printOnScroll(0, REGULAR, 0,  "URL: m32.local");
  MorseOutput::printOnScroll(1, REGULAR, 0,  "IP:");
  MorseOutput::printOnScroll(2, REGULAR, 0, WiFi.localIP().toString(), true);
  unsigned long wifiTimeout = millis() + 300000UL;  // 5 minute timeout
  while (true) {
       server.handleClient();
       delay(10);
       Buttons::volButton.Update();
       if (Buttons::volButton.clicks) {
           break;
       }
       if (millis() > wifiTimeout) {
           MorseOutput::printOnScroll(1, BOLD, 0, "Timeout!");
           delay(1000);
           break;
       }
   }
   shutdownWiFi();
}
#endif

boolean MorseWiFi::wifiConnect() {                   // connect to local WLAN
  // Connect to WiFi network
  if (MorsePreferences::wlanSSID == "")
      return internal::errorConnect(String("WiFi Not Conf"));
  //DEBUG("SSID: " + MorsePreferences::wlanSSID + " PW: >" +  MorsePreferences::wlanPassword + "<");
  WiFi.begin(MorsePreferences::wlanSSID.c_str(), MorsePreferences::wlanPassword.c_str());
  // Wait for connection
  long unsigned int wait = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    if ((millis() - wait) > 11000)
      return internal::errorConnect(String("No WiFi:"));
  }
  //DEBUG("Connected to " + String(p_wlanSSID));
  //DEBUG("IP address: " + String(WiFi.localIP()));
  internal::startMDNS();
  return true;
}


boolean internal::errorConnect(String msg) {
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Not connected");
  MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, msg);
  MorseOutput::printOnScroll(1, REGULAR, 0, MorsePreferences::wlanSSID);
  delay(3500);
  if (m32protocol) {
    MorseJSON::jsonCreate("message", "Not connected " + msg, "");
  }
  return false;
}

void internal::startMDNS() {
  /*use mdns for host name resolution*/
  if (!MDNS.begin(MorseWiFi::host)) {         //http://m32.local
    //DEBUG("Error setting up MDNS responder!");
    int i = 5;
    while (i) {
        i--;
        delay(1000);
        //DEBUG("mDNS responder retry");

      if (MDNS.begin(MorseWiFi::host))
        break;
    }
  }
}


void MorseWiFi::uploadFile() {
  volatile bool uploadDone = false;
  if (! wifiConnect())
    return;
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", uploadLoginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

server.on("/update", HTTP_POST, [&uploadDone](){
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK");
    uploadDone = true;                          // signal the loop to exit
    },
    internal::handleFileUpload
  );

  server.onNotFound([]() {                              // If the client requests any URI
    if (!internal::handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();                           // Actually start the server
  //DEBUG("HTTP server started");
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Waiting f. Upload ");
  MorseOutput::printOnScroll(0, REGULAR, 0,  "URL: m32.local");
  MorseOutput::printOnScroll(1, REGULAR, 0,  "IP:");
  MorseOutput::printOnScroll(2, REGULAR, 0, WiFi.localIP().toString());
  unsigned long wifiTimeout = millis() + 300000UL;  // 5 minute timeout
  while (!uploadDone) {
       server.handleClient();
       delay(5);
       Buttons::volButton.Update();
       if (Buttons::volButton.clicks) {
           break;
       }
       if (millis() > wifiTimeout) {
           MorseOutput::printOnScroll(1, BOLD, 0, "Timeout!");
           delay(1000);
           break;
       }
   }
   shutdownWiFi();
}


String internal::getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}


bool internal::handleFileRead(String path) { // send the right file to the client (if it exists)
  //DEBUG("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = internal::getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {     // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                            // If there's a compressed version available
      path += ".gz";                                          // Use the compressed verion
    File file = SPIFFS.open(path, "r");                       // Open the file
    MorseWiFi::server.streamFile(file, contentType);                     // Send it to the client
    file.close();                                             // Close the file again
    //DEBUG(String("\tSent file: ") + path);
    return true;
  }
  //DEBUG(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void internal::handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = MorseWiFi::server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    //DEBUG("handleFileUpload Name: " + filename);
        MorseWiFi::fsUploadFile = SPIFFS.open("/player.txt", "w");     // Open the file for writing in SPIFFS (create if it doesn't exist)
        filename = String();
    } else if(upload.status == UPLOAD_FILE_WRITE){
      if(MorseWiFi::fsUploadFile)
        MorseWiFi::fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
    } else if(upload.status == UPLOAD_FILE_END){
      if(MorseWiFi::fsUploadFile) {                                    // If the file was successfully created
        MorseWiFi::fsUploadFile.close();                               // Close the file again
        MorsePreferences::fileWordPointer = 0;                         // reset word counter for file player
//DEBUG("fileWordPointer @ file upload: " + String(MorsePreferences::fileWordPointer));
        MorsePreferences::writeWordPointer();
        // Fresh file invalidates prior progress in every chapter
        for (int i = 0; i < MAX_FILE_PARTS; i++)
          MorsePreferences::fileParts[i].wordPointer = 0;
        MorsePreferences::scanFileParts();          // ADD: detect multipart
        MorsePreferences::writeFilePartData();      // ADD: persist to NVS
//DEBUG("handleFileUpload Size: " + String(upload.totalSize));
    } else {
      MorseWiFi::server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
