// Local regression fixture; no network or user profile is used.
const {app, BrowserWindow} = require('electron');
const fs = require('node:fs');
app.commandLine.appendSwitch('ozone-platform', 'wayland');
app.disableHardwareAcceleration();
app.whenReady().then(async () => {
  const outsideWindow=process.env.SMOOTH_AUTOSCROLL_TEST_VARIANT==='window';
  const win = new BrowserWindow({x:0,y:0,width:outsideWindow ? 600 : 800, height:600, frame:false,
    fullscreen:!outsideWindow, webPreferences:{contextIsolation:true, sandbox:true}});
  await win.loadURL('data:text/html;charset=utf-8,' + encodeURIComponent(`
    <style>html,body{margin:0;height:100%;overflow:hidden;cursor:crosshair}
    #pane{position:absolute;left:100px;width:280px;height:100vh;overflow-y:scroll;overflow-x:hidden}
    #sidebar{position:absolute;left:400px;right:0;height:100vh;background:#555}
    #content{height:10000px;background:linear-gradient(white,gray)}</style>
    <div id="pane"><div id="content">Vertical only scrolling fixture</div></div>
    <div id="sidebar">Non-scrolling sidebar</div>
    <script>document.querySelector('#pane').scrollTop=5000;
    window.wheels=[];addEventListener('wheel',e=>wheels.push([e.deltaX,e.deltaY]),{passive:true});</script>
  `));
  setInterval(async () => {
    try {
      const data = await win.webContents.executeJavaScript(`JSON.stringify({
        y:document.querySelector('#pane').scrollTop,
        x:document.querySelector('#pane').scrollLeft,
        vertical:wheels.filter(e=>e[1]!==0).length,
        horizontal:wheels.filter(e=>e[0]!==0).length
      })`);
      const destination=process.env.SMOOTH_AUTOSCROLL_BROWSER_STATE;
      fs.writeFileSync(destination+'.tmp',data);
      fs.renameSync(destination+'.tmp',destination);
    } catch (_) {}
  }, 40);
});
app.on('window-all-closed',()=>app.quit());
