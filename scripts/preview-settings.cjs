'use strict';
// Offline layout review of the exact generated template. No native/phone write.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..'),version=require('../package.json').version;
const out=path.join(process.env.PEBBLE_QA_OUTPUT_ROOT||path.join(root,'build/qa'),'phone-preview.html');
function mockBridge(){
  var saved={version:2,textSize:'follow',appearance:'light',lastExplicit:'light'};
  window.Pebble={targets:function(){return Promise.resolve([{id:'plugin'}]);},
    addEventListener:function(name,fn){if(name==='ready')document.addEventListener('DOMContentLoaded',function(){fn({target:'plugin'});});},
    sendMessage:function(target,message){
      if(message.type==='saveUiPreferences')saved=message.settings;
      return Promise.resolve({ok:true,settings:saved,saved:true,delivery:{applied:false,pending:true}});
    }};
}
const html=fs.readFileSync(path.join(root,'config.html'),'utf8')
  .replace('<body>','<body><aside style="padding:8px 16px;text-align:center;font-size:12px;background:var(--card);color:var(--muted)">Local layout preview · Mock save only · No watch or phone storage</aside>')
  .replace('<script>','<script>('+mockBridge.toString()+')();</script><script>');
fs.mkdirSync(path.dirname(out),{recursive:true});fs.writeFileSync(out,html);console.log(out);
