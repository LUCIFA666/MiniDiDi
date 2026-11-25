import * as THREE from 'three';
import {OrbitControls} from 'three/addons/OrbitControls.js';
const $=id=>document.getElementById(id);
const colors=['#d7e8a0','#efbf77','#8dc6cf','#bbaccf','#dfa59d','#e1c1f2'];
const statusText={idle:'空闲待命',pickup:'前往接客',occupied:'正在载客'};
const orderText={waiting:'等待派车',assigned:'前往接客',onboard:'行程中',completed:'已送达'};
let state=null,selected='V01',view='3d',comparison=null,toastTimer;
let renderer,scene,camera,controls,city,routeGroup,orderGroup,carMeshes=new Map(),lastMapVersion;
let canvas2d=document.createElement('canvas'),ctx=canvas2d.getContext('2d');
const host=$('canvas-host');
function toast(message){$('toast').textContent=message;$('toast').hidden=false;clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').hidden=true,4500);}
async function api(path,data){const response=await fetch('/api/'+path,data===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});const j=await response.json();if(!response.ok)throw Error(j.error||'请求失败');return j;}
function el(tag,cls,text){let e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e;}
function box(w,h,d,color,x,y,z,roughness=.8){const m=new THREE.Mesh(new THREE.BoxGeometry(w,h,d),new THREE.MeshStandardMaterial({color,roughness}));m.position.set(x,y,z);return m;}
function init3D(){
  try{
    renderer=new THREE.WebGLRenderer({antialias:true,alpha:false});renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.setClearColor('#132a30');
    renderer.outputColorSpace=THREE.SRGBColorSpace;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1.35;host.append(renderer.domElement);
    scene=new THREE.Scene();scene.fog=new THREE.Fog('#132a30',55,105);
    camera=new THREE.PerspectiveCamera(39,1,.1,180);camera.position.set(34,34,37);
    controls=new OrbitControls(camera,renderer.domElement);controls.target.set(11,0,9);controls.enableDamping=true;controls.maxPolarAngle=Math.PI*.43;controls.minDistance=17;controls.maxDistance=65;controls.maxTargetRadius=15;
    scene.add(new THREE.HemisphereLight('#d9efe2','#344e50',2.5));let sun=new THREE.DirectionalLight('#fff6de',3);sun.position.set(10,25,15);scene.add(sun);
    city=new THREE.Group();routeGroup=new THREE.Group();orderGroup=new THREE.Group();scene.add(city,routeGroup,orderGroup);
    renderer.domElement.addEventListener('pointerdown',e=>{renderer.domElement.dataset.pointerX=e.clientX;renderer.domElement.dataset.pointerY=e.clientY;});
    renderer.domElement.addEventListener('pointerup',e=>{
      if(Math.hypot(e.clientX-Number(renderer.domElement.dataset.pointerX),e.clientY-Number(renderer.domElement.dataset.pointerY))>5)return;
      const rect=renderer.domElement.getBoundingClientRect(),mouse=new THREE.Vector2((e.clientX-rect.left)/rect.width*2-1,-(e.clientY-rect.top)/rect.height*2+1),ray=new THREE.Raycaster();ray.setFromCamera(mouse,camera);
      const hits=ray.intersectObjects([...carMeshes.values()],true);if(hits.length){let m=hits[0].object;while(m&&!m.userData.id)m=m.parent;if(m){selected=m.userData.id;renderPanel();renderRoutes();}}
    });
  }catch(e){renderer=null;view='2d';$('view-3d').disabled=true;$('map-hint').textContent='当前设备使用 2D 视图';toast('WebGL 不可用，已切换到 2D 视图');}
}
function clearGroup(group){if(!group)return;while(group.children.length){const c=group.children[0];group.remove(c);c.traverse(o=>{o.geometry?.dispose();if(Array.isArray(o.material))o.material.forEach(m=>m.dispose());else o.material?.dispose();});}}
function buildCity(map){
  if(!renderer)return;clearGroup(city);lastMapVersion=map.version;
  const base=box(map.width+.7,.36,map.height+.7,'#263f43',(map.width-1)/2,-.31,(map.height-1)/2);city.add(base);
  const road=box(map.width,.08,map.height,'#365251',(map.width-1)/2,-.08,(map.height-1)/2);city.add(road);
  let grid=new THREE.GridHelper(map.width,map.width,'#4a6860','#3f5c56');grid.position.set((map.width-1)/2,-.025,(map.height-1)/2);grid.scale.z=map.height/map.width;city.add(grid);
  // Deterministic procedural geometry: no external models, imagery or network dependencies.
  for(let y=2;y<map.height;y+=6)for(let x=2;x<map.width;x+=6){
    city.add(box(2.65,.17,2.65,'#769282',x+.5,.035,y+.5));
    const h=1.2+((x*17+y*13)%7)*.28;
    city.add(box(1.62,h,1.62,(x+y)%3===0?'#9db9a4':'#b9cbb4',x+.4,h/2+.13,y+.45));
    city.add(box(1.72,.09,1.72,'#d9e0c8',x+.4,h+.17,y+.45));
    city.add(box(.8,.3,.7,'#809b8c',x+.4,h+.35,y+.45));
    for(let floor=.5;floor<h;floor+=.5){city.add(box(1.64,.07,1.64,'#76948a',x+.4,floor,y+.45));}
  }
  for(let x=0;x<map.width;x+=6)for(let y=0;y<map.height;y+=6){
    const t=new THREE.Mesh(new THREE.SphereGeometry(.27,7,6),new THREE.MeshStandardMaterial({color:'#729875',roughness:1}));t.position.set(x+.65,.43,y+.62);city.add(t);
  }
  let ring=new THREE.LineLoop(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(-.8,-.05,-.8),new THREE.Vector3(map.width-.2,-.05,-.8),new THREE.Vector3(map.width-.2,-.05,map.height-.2),new THREE.Vector3(-.8,-.05,map.height-.2)]),new THREE.LineBasicMaterial({color:'#719280'}));city.add(ring);
}
function makeCar(v,index){const group=new THREE.Group();group.userData.id=v.id;const color=colors[index%colors.length];group.add(box(.82,.29,.45,color,0,.22,0,.45));group.add(box(.43,.23,.4,'#d6e2d4',-.03,.45,0,.35));group.add(box(.23,.17,.405,'#486967',.055,.46,0,.25));for(let x of [-.25,.25])for(let z of [-.24,.24]){const wheel=new THREE.Mesh(new THREE.CylinderGeometry(.1,.1,.06,10),new THREE.MeshStandardMaterial({color:'#233d3c'}));wheel.rotation.x=Math.PI/2;wheel.position.set(x,.16,z);group.add(wheel);}const marker=new THREE.Mesh(new THREE.RingGeometry(.56,.6,28),new THREE.MeshBasicMaterial({color,transparent:true,opacity:.55,side:THREE.DoubleSide}));marker.rotation.x=-Math.PI/2;marker.position.y=.04;marker.userData.statusRing=true;group.add(marker);group.position.set(v.position.x,0,v.position.y);scene.add(group);carMeshes.set(v.id,group);return group;}
function renderRoutes(){
  if(!renderer||!state)return;clearGroup(routeGroup);clearGroup(orderGroup);
  for(const v of state.vehicles){
    let route=v.route||[];if(v.id===selected&&route.length>1){let points=route.map(p=>new THREE.Vector3(p.x,.09,p.y));routeGroup.add(new THREE.Line(new THREE.BufferGeometry().setFromPoints(points),new THREE.LineBasicMaterial({color:'#dcdfa2',transparent:true,opacity:.9})));}
  }
  for(const o of state.orders.filter(o=>o.status==='waiting'||o.status==='assigned').slice(-30)){
    let mat=new THREE.MeshStandardMaterial({color:'#f0c77e',emissive:'#946329',emissiveIntensity:.25});let marker=new THREE.Mesh(new THREE.CylinderGeometry(.12,.18,.65,8),mat);marker.position.set(o.pickup.x,.42,o.pickup.y);orderGroup.add(marker);
  }
  const car=state.vehicles.find(v=>v.id===selected);if(car&&car.order){const order=state.orders.find(o=>o.id===car.order);if(order){const goal=order.picked<0?order.pickup:order.dropoff;let marker=new THREE.Mesh(new THREE.TorusGeometry(.38,.04,8,24),new THREE.MeshBasicMaterial({color:'#f3de9f'}));marker.rotation.x=-Math.PI/2;marker.position.set(goal.x,.11,goal.y);orderGroup.add(marker);}}
}
function applyView(){if(view==='3d'&&renderer){canvas2d.remove();host.append(renderer.domElement);$('view-3d').classList.add('active');$('view-2d').classList.remove('active');}else{renderer?.domElement.remove();host.append(canvas2d);$('view-2d').classList.add('active');$('view-3d').classList.remove('active');}$('map-hint').textContent=view==='3d'&&renderer?'拖动旋转 · 滚轮缩放 · 点击车辆查看路线':'点击下方车队卡片查看路线';resize();}
function resize(){const {width,height}=host.getBoundingClientRect();if(!width||!height)return;if(renderer){renderer.setSize(width,height);camera.aspect=width/height;camera.updateProjectionMatrix();}canvas2d.width=width*devicePixelRatio;canvas2d.height=height*devicePixelRatio;canvas2d.style.width=width+'px';canvas2d.style.height=height+'px';}
function draw2D(){if(!state)return;const w=canvas2d.width,h=canvas2d.height,scale=Math.min((w-100*devicePixelRatio)/state.map.width,(h-85*devicePixelRatio)/state.map.height),ox=(w-scale*state.map.width)/2,oy=(h-scale*state.map.height)/2;ctx.fillStyle='#132a30';ctx.fillRect(0,0,w,h);ctx.fillStyle='#35524f';ctx.fillRect(ox,oy,state.map.width*scale,state.map.height*scale);ctx.strokeStyle='#45625a';ctx.lineWidth=devicePixelRatio*.4;for(let x=0;x<=state.map.width;x++){ctx.beginPath();ctx.moveTo(ox+x*scale,oy);ctx.lineTo(ox+x*scale,oy+state.map.height*scale);ctx.stroke();}for(let y=0;y<=state.map.height;y++){ctx.beginPath();ctx.moveTo(ox,oy+y*scale);ctx.lineTo(ox+state.map.width*scale,oy+y*scale);ctx.stroke();}ctx.fillStyle='#9fb9a2';for(let p of state.map.obstacles)ctx.fillRect(ox+p.x*scale+1,oy+p.y*scale+1,scale-2,scale-2);let v=state.vehicles.find(c=>c.id===selected);if(v?.route.length){ctx.strokeStyle='#e0d89c';ctx.lineWidth=2*devicePixelRatio;ctx.beginPath();v.route.forEach((p,i)=>ctx[i?'lineTo':'moveTo'](ox+(p.x+.5)*scale,oy+(p.y+.5)*scale));ctx.stroke();}for(let o of state.orders.filter(o=>o.status==='waiting'||o.status==='assigned')){ctx.fillStyle='#efc477';ctx.beginPath();ctx.arc(ox+(o.pickup.x+.5)*scale,oy+(o.pickup.y+.5)*scale,scale*.16,0,Math.PI*2);ctx.fill();}state.vehicles.forEach((c,i)=>{ctx.fillStyle=colors[i%colors.length];ctx.beginPath();ctx.arc(ox+(c.position.x+.5)*scale,oy+(c.position.y+.5)*scale,scale*.35,0,Math.PI*2);ctx.fill();ctx.strokeStyle=c.status==='idle'?'#b1d8a3':c.status==='pickup'?'#f2bb6d':'#8cbecb';ctx.lineWidth=2*devicePixelRatio;ctx.stroke();ctx.fillStyle='#e2ebd9';ctx.font=`${9*devicePixelRatio}px monospace`;ctx.fillText(c.id,ox+(c.position.x+.8)*scale,oy+(c.position.y+.2)*scale);});}
function renderPhone(){const canvas=$('phone-pose'),c=canvas.getContext('2d');c.clearRect(0,0,110,60);c.strokeStyle='#e8eee3';for(let x=5;x<110;x+=10){c.beginPath();c.moveTo(x,0);c.lineTo(x,60);c.stroke();}for(let y=0;y<60;y+=10){c.beginPath();c.moveTo(0,y);c.lineTo(110,y);c.stroke();}const p=state.phone?.pose;if(!p)return;const x=55+Math.max(-48,Math.min(48,p.east*4)),y=30-Math.max(-24,Math.min(24,p.north*4));c.fillStyle=state.phone.connected?'#729a82':'#b6c3b0';c.save();c.translate(x,y);c.rotate(p.yaw);c.beginPath();c.moveTo(0,-6);c.lineTo(4,5);c.lineTo(-4,5);c.closePath();c.fill();c.restore();}
function renderPanel(){
  if(!state)return;const m=state.metrics;
  $('metric-fleet').textContent=state.vehicles.filter(c=>c.connected).length;$('fleet-caption').textContent=`${state.vehicles.filter(c=>c.kind==='virtual').length} 辆数字车 · ${m.active_vehicles} 辆执行任务`;
  $('metric-complete').textContent=m.completed;$('complete-caption').textContent=`共 ${m.orders} 笔订单 · 完成率 ${(m.completion_rate*100).toFixed(0)}%`;
  $('metric-wait').textContent=m.mean_wait_ticks.toFixed(1);$('metric-distance').textContent=m.total_distance;
  $('safety').textContent=m.collisions===0?'● 零碰撞':`● ${m.collisions} 次碰撞`;$('clock').textContent=`T + ${String(state.tick).padStart(4,'0')} s`;
  $('pause').textContent=state.paused?'▶ 继续仿真':'Ⅱ 暂停仿真';$('strategy').value=state.strategy;$('speed').value=String(state.speed);
  $('pending-count').textContent=`${m.pending} 待分配`;$('fleet-summary').textContent=`${m.active_vehicles} 执行任务 / ${state.vehicles.length} 辆`;
  const rows=state.orders.slice(-18).reverse().map(o=>{let row=el('div','order-row'),top=el('div','order-top');top.append(el('span','order-id',`# ${String(o.id).padStart(3,'0')}`),el('span','order-badge '+o.status,orderText[o.status]));let trip=el('div','trip');trip.append(el('i'),el('span','',`街区 ${o.pickup.x}, ${o.pickup.y}`),el('span','arrow','→'),el('span','',`${o.dropoff.x}, ${o.dropoff.y}`));let meta=el('div','order-meta');meta.append(el('span','',o.car>=0?(state.vehicles[o.car]?.id||'车辆'):'等待可用车辆'),el('span','',`T + ${o.arrival} s`));row.append(top,trip,meta);return row;});$('orders').replaceChildren(...rows);
  $('fleet').replaceChildren(...state.vehicles.map((v,i)=>{let button=el('button','vehicle'+(selected===v.id?' active':''));let top=el('div','vehicle-top');let swatch=el('i','car-swatch');swatch.style.background=colors[i%colors.length];top.append(el('span','',v.id),swatch);button.append(top,el('div','vehicle-status',v.connected?statusText[v.status]:'连接中断 · 保留占位'),el('div','vehicle-stats',`${v.completed} 单 / ${v.distance} m`));button.addEventListener('click',()=>{selected=v.id;renderPanel();renderRoutes();});return button;}));
  const current=state.vehicles.find(v=>v.id===selected);if(current)$('selection-info').textContent=`${current.id} · ${statusText[current.status]} · 坐标 (${current.cell.x}, ${current.cell.y})${current.order?` · 订单 #${current.order} · 目标 (${current.goal.x}, ${current.goal.y})`:' · 等待下一次派单'}`;
  const matlab=state.vehicles.find(v=>v.kind==='matlab');$('matlab-status').textContent=matlab?(matlab.connected?'已连接':'连接中断'):'未接入';$('matlab-status').classList.toggle('online',!!matlab?.connected);$('matlab-detail').textContent=matlab?`M01 · ${matlab.completed} 单 · 位姿 (${matlab.position.x.toFixed(2)}, ${matlab.position.y.toFixed(2)})`:'独立运动控制 · 网络状态同步 · M01';
  const phone=state.phone;$('phone-status').textContent=phone.connected?'已连接':phone.pose?'连接中断':'未接入';$('phone-status').classList.toggle('online',phone.connected);$('phone-detail').textContent=phone.pose?`独立实验区 · E ${Number(phone.pose.east).toFixed(2)} m / N ${Number(phone.pose.north).toFixed(2)} m · ${phone.samples} 帧`:'IMU + GNSS · 本地位姿估计 · P01';renderPhone();
}
async function poll(){try{state=await api('state');if(!state.vehicles.some(v=>v.id===selected))selected=state.vehicles[0]?.id||'V01';$('connection').className='connection';$('connection').replaceChildren(el('i'),document.createTextNode(state.paused?'仿真已暂停':'自主运行中'));if(renderer){if(lastMapVersion!==state.map.version)buildCity(state.map);const ids=new Set(state.vehicles.map(v=>v.id));for(const [id,m]of carMeshes)if(!ids.has(id)){scene.remove(m);m.traverse(o=>{o.geometry?.dispose();o.material?.dispose();});carMeshes.delete(id);}state.vehicles.forEach((v,i)=>{if(!carMeshes.has(v.id))makeCar(v,i);});renderRoutes();}renderPanel();}catch(e){$('connection').className='connection error';$('connection').replaceChildren(el('i'),document.createTextNode('连接中断 · 自动重试'));}setTimeout(poll,400);}
async function control(data){try{await api('control',data);}catch(e){toast(e.message);}}
$('pause').onclick=()=>control({paused:!state?.paused});$('speed').onchange=e=>control({speed:Number(e.target.value)});$('strategy').onchange=e=>control({strategy:e.target.value});
$('reset').onclick=async()=>{try{await api('reset',{seed:state?.seed||42,strategy:state?.strategy||'balanced'});toast('场景已重置；已接入设备将自动重新注册');}catch(e){toast(e.message);}};
$('view-3d').onclick=()=>{view='3d';applyView();};$('view-2d').onclick=()=>{view='2d';applyView();};
function download(name,data){const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));let a=el('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
async function runComparison(){
  const dialog=$('comparison');if(!dialog.open)dialog.showModal();$('comparison-body').replaceChildren(el('div','empty','正在运行两组独立实验，请稍候…'));$('download-comparison').disabled=true;$('compare-seed').textContent='';
  try{comparison=await api('compare',{seed:state?.seed||42,steps:1200});let table=el('table','comparison-table'),head=el('tr');['指标','最近空车优先','批量均衡调度'].forEach(t=>head.append(el('th','',t)));table.append(head);
    const [a,b]=comparison.results.map(r=>r.metrics);for(const [label,key,unit,digits] of [['平均接客等待','mean_wait_ticks',' s',1],['总行驶里程','total_distance',' m',0],['完成订单','completed',' / 80',0],['累计未上车等待','outstanding_wait_ticks',' s',0],['碰撞次数','collisions','',0]]){let row=el('tr');row.append(el('td','',label));for(const m of [a,b])row.append(el('td','',Number(m[key]).toFixed(digits)+unit));table.append(row);}
    $('comparison-body').replaceChildren(table,el('p','compare-note',comparison.note+' “均衡”是待检验的策略，不保证在每个种子下都优于基线。'));$('compare-seed').textContent=`SEED ${comparison.seed} / ${comparison.steps} TICKS`;$('download-comparison').disabled=false;
  }catch(e){$('comparison-body').replaceChildren(el('p','compare-note','实验失败：'+e.message));}
}
$('compare').onclick=runComparison;$('rail-compare').onclick=runComparison;$('close-dialog').onclick=()=>$('comparison').close();$('download-comparison').onclick=()=>download(`MiniDiDi-compare-${comparison.seed}.json`,comparison);
init3D();applyView();new ResizeObserver(resize).observe(host);
let lastFrame=performance.now();function animate(now){let dt=Math.min(.1,(now-lastFrame)/1000);lastFrame=now;requestAnimationFrame(animate);if(view==='3d'&&renderer){if(state)for(let v of state.vehicles){let m=carMeshes.get(v.id);if(m){const ring=m.children.find(o=>o.userData.statusRing);if(ring)ring.material.color.set(v.status==='idle'?'#b1d8a3':v.status==='pickup'?'#f2bb6d':'#8cbecb');let a=1-Math.exp(-dt*9);m.position.x+=(v.position.x-m.position.x)*a;m.position.z+=(v.position.y-m.position.z)*a;let target=-v.yaw,delta=Math.atan2(Math.sin(target-m.rotation.y),Math.cos(target-m.rotation.y));m.rotation.y+=delta*a;}}controls.update();renderer.render(scene,camera);}else draw2D();}requestAnimationFrame(animate);poll();
