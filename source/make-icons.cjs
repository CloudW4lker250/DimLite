// Code-generated monochrome icons; no image dependencies are required.
const fs = require('node:fs');
const path = require('node:path');
const sizes = [16, 32, 48, 64];
function segmentDistance(x,y,ax,ay,bx,by) {
  const dx=bx-ax,dy=by-ay,t=Math.max(0,Math.min(1,((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy)));
  return Math.hypot(x-ax-t*dx,y-ay-t*dy);
}
function makeIcon(filename, kind) {
  const frames=sizes.map(n=>{
    const stride=Math.ceil(n/32)*4,data=Buffer.alloc(40+n*n*4+stride*n);
    data.writeUInt32LE(40,0);data.writeInt32LE(n,4);data.writeInt32LE(n*2,8);
    data.writeUInt16LE(1,12);data.writeUInt16LE(32,14);data.writeUInt32LE(n*n*4,20);
    for(let y=0;y<n;y++)for(let x=0;x<n;x++) {
      let sum=0,hits=0;
      for(let sy=0;sy<4;sy++)for(let sx=0;sx<4;sx++) {
        const px=(x+(sx+.5)/4)/n-.5,py=(y+(sy+.5)/4)/n-.5,d=Math.hypot(px,py);
        let ink=d>.185&&d<.255;
        for(let k=0;k<8;k++) {
          const a=k*Math.PI/4,c=Math.cos(a),s=Math.sin(a);
          ink ||= segmentDistance(px,py,c*.335,s*.335,c*.445,s*.445)<.027;
        }
        let value;
        if(ink) value=kind==='app'?128:kind==='white'?255:20;
        if(value!==undefined){sum+=value;hits++;}
      }
      const i=40+((n-1-y)*n+x)*4;
      if(hits) {const v=Math.round(sum/hits);data[i]=data[i+1]=data[i+2]=v;data[i+3]=Math.round(255*hits/16);}
      else data[40+n*n*4+(n-1-y)*stride+(x>>3)]|=0x80>>(x&7);
    }
    return data;
  });
  const header=Buffer.alloc(6+16*sizes.length);
  header.writeUInt16LE(1,2);header.writeUInt16LE(sizes.length,4);
  let offset=header.length;
  frames.forEach((frame,i)=>{const p=6+i*16;header[p]=header[p+1]=sizes[i];header.writeUInt16LE(1,p+4);header.writeUInt16LE(32,p+6);header.writeUInt32LE(frame.length,p+8);header.writeUInt32LE(offset,p+12);offset+=frame.length;});
  fs.writeFileSync(path.join(__dirname,filename),Buffer.concat([header,...frames]));
}
makeIcon('DimLite.ico','app');
makeIcon('TrayWhite.ico','white');
makeIcon('TrayBlack.ico','black');
