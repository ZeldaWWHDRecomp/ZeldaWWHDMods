"""Stdlib port of the authored bounded contour tracer; no learned geography.

Geometry and source-centre validation preserve the legacy algorithm. Rasterisation
uses explicit even-odd scan conversion and separable Lanczos-3, rather than Pillow;
byte equivalence to Pillow is not assumed and must be checked visually.
"""
import math
import json


def contours(mask,width,height):
    pairs={1:[(0,3)],2:[(0,1)],3:[(3,1)],4:[(1,2)],5:[(0,1),(2,3)],6:[(0,2)],7:[(3,2)],8:[(2,3)],9:[(0,2)],10:[(0,3),(1,2)],11:[(1,2)],12:[(3,1)],13:[(0,1)],14:[(0,3)]}
    def sample(x,y): return 0 if x<1 or y<1 or x>width or y>height else int(mask[(y-1)*width+x-1])
    graph={}
    for y in range(height+1):
        for x in range(width+1):
            case=sample(x,y)+2*sample(x+1,y)+4*sample(x+1,y+1)+8*sample(x,y+1)
            pts=[(2*x+1,2*y),(2*x+2,2*y+1),(2*x+1,2*y+2),(2*x,2*y+1)]
            for ia,ib in pairs.get(case,[]):
                a,b=pts[ia],pts[ib];graph.setdefault(a,[]).append(b);graph.setdefault(b,[]).append(a)
    result=[]
    while graph:
        start=next(iter(graph));current=start;previous=None;points=[]
        while True:
            if len(points)>4*(width+1)*(height+1): raise ValueError('Invalid contour cycle')
            points.append(current)
            neighbors=graph[current]
            if len(neighbors)!=2: raise ValueError('Nonmanifold contour')
            following=neighbors[0] if neighbors[0]!=previous else neighbors[1]
            previous,current=current,following
            if current==start: break
        for point in points: del graph[point]
        original=[(x/2-.5,y/2-.5) for x,y in points];p=original[:]
        for _ in range(2):
            p=[tuple(.25*p[i-1][axis]+.5*v[axis]+.25*p[(i+1)%len(p)][axis] for axis in (0,1)) for i,v in enumerate(p)]
        fair=[]
        for source,target in zip(original,p):
            dx,dy=target[0]-source[0],target[1]-source[1];factor=min(1,.35/max(math.hypot(dx,dy),1e-9))
            fair.append((source[0]+dx*factor,source[1]+dy*factor))
        keep=[]
        for i,point in enumerate(fair):
            a=(point[0]-fair[i-1][0],point[1]-fair[i-1][1]);b=(fair[(i+1)%len(fair)][0]-point[0],fair[(i+1)%len(fair)][1]-point[1])
            if abs(a[0]*b[1]-a[1]*b[0])>1e-8: keep.append(point)
        if len(keep)>=3: result.append(keep)
    return result


def rounded_path(p):
    before=[];after=[]
    for i,v in enumerate(p):
        a=(p[i-1][0]-v[0],p[i-1][1]-v[1]);b=(p[(i+1)%len(p)][0]-v[0],p[(i+1)%len(p)][1]-v[1])
        la,lb=math.hypot(*a),math.hypot(*b)
        if not la or not lb: raise ValueError('Degenerate contour edge')
        radius=min(.18,la*.25,lb*.25)
        before.append((v[0]+a[0]/la*radius,v[1]+a[1]/la*radius));after.append((v[0]+b[0]/lb*radius,v[1]+b[1]/lb*radius))
    def fmt(v): return f'{v[0]:.5f},{v[1]:.5f}'
    commands=['M'+fmt(after[-1])];samples=[after[-1]]
    for v,a,b in zip(p,before,after):
        commands.extend(['L'+fmt(a),'Q'+fmt(v)+' '+fmt(b)]);samples.append(a)
        for i in range(1,9):
            t=i/8;samples.append(tuple((1-t)**2*a[k]+2*(1-t)*t*v[k]+t*t*b[k] for k in (0,1)))
    return ' '.join(commands)+' Z',samples


def inside(x,y,polygon):
    result=False
    for a,b in zip(polygon,polygon[1:]+polygon[:1]):
        if a[1]!=b[1] and (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]: result=not result
    return result


def intersections(y,polygons,scale):
    points=[]
    for polygon in polygons:
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            if (a[1]*scale>y)!=(b[1]*scale>y):
                points.append(a[0]*scale+(b[0]-a[0])*(y-a[1]*scale)/(b[1]-a[1]))
    points.sort()
    if len(points)%2: raise ValueError('Odd contour scan intersections')
    return points


def lanczos_half(rgba,size):
    # Alpha-premultiplied filtering avoids dark fringes around transparent coast.
    out_size=size//2
    def sinc(x): return 1 if x==0 else math.sin(math.pi*x)/(math.pi*x)
    table=[]
    for x in range(out_size):
        center=(x+.5)*2;weights=[]
        for i in range(max(0,math.ceil(center-6-.5)),min(size,math.floor(center+6-.5)+1)):
            d=(i+.5-center)/2;weight=sinc(d)*sinc(d/3) if abs(d)<3 else 0
            weights.append((i,weight))
        total=sum(w for _,w in weights);table.append([(i,w/total) for i,w in weights])
    weight_keys=[tuple(weight for _,weight in weights) for weights in table]
    horizontal_cache={};vertical_cache={}
    horizontal=bytearray(out_size*size*4)
    for y in range(size):
        for x,weights in enumerate(table):
            start=(y*size+weights[0][0])*4;end=(y*size+weights[-1][0]+1)*4
            pixel=bytes(rgba[start:start+4])
            if rgba[start:end]==pixel*len(weights):
                key=(weight_keys[x],pixel)
                if key not in horizontal_cache:
                    sums=[0.,0.,0.,0.]
                    for _,weight in weights:
                        for channel in range(3): sums[channel]+=pixel[channel]*pixel[3]/255*weight
                        sums[3]+=pixel[3]*weight
                    horizontal_cache[key]=bytes(max(0,min(255,round(value))) for value in sums)
                offset=(x*size+y)*4
                horizontal[offset:offset+4]=horizontal_cache[key]
                continue
            sums=[0.,0.,0.,0.]
            for i,weight in weights:
                offset=(y*size+i)*4;a=rgba[offset+3]
                for channel in range(3): sums[channel]+=rgba[offset+channel]*a/255*weight
                sums[3]+=a*weight
            offset=(x*size+y)*4
            for channel in range(4): horizontal[offset+channel]=max(0,min(255,round(sums[channel])))
    out=bytearray(out_size*out_size*4)
    for y,weights in enumerate(table):
        for x in range(out_size):
            start=(x*size+weights[0][0])*4;end=(x*size+weights[-1][0]+1)*4
            pixel=bytes(horizontal[start:start+4])
            if horizontal[start:end]==pixel*len(weights):
                key=(weight_keys[y],pixel)
                if key not in vertical_cache:
                    sums=[0.,0.,0.,0.]
                    for _,weight in weights:
                        for channel in range(4): sums[channel]+=pixel[channel]*weight
                    a=max(0,min(255,round(sums[3])))
                    vertical_cache[key]=bytes([max(0,min(255,round(sums[channel]*255/a))) if a else 0 for channel in range(3)]+[a])
                offset=(y*out_size+x)*4
                out[offset:offset+4]=vertical_cache[key]
                continue
            sums=[0.,0.,0.,0.]
            for i,weight in weights:
                offset=(x*size+i)*4
                for channel in range(4): sums[channel]+=horizontal[offset+channel]*weight
            offset=(y*out_size+x)*4;a=max(0,min(255,round(sums[3])));out[offset+3]=a
            for channel in range(3): out[offset+channel]=max(0,min(255,round(sums[channel]*255/a))) if a else 0
    return bytes(out)


def trace(width,height,rgba,size=1024):
    if width!=height or not 1<=width<=512 or len(rgba)!=width*height*4 or not 120<=size<=1024: raise ValueError('Expected bounded square indexed map')
    pixels=[tuple(rgba[i:i+4]) for i in range(0,len(rgba),4)]
    colors=sorted({p for p in pixels if p[3]>127})
    if not 1<=len(colors)<=32: raise ValueError('Expected small visible indexed palette')
    colors.sort(key=lambda c:-(.2126*c[0]+.7152*c[1]+.0722*c[2]))
    regions=[];reconstructed=[(0,0,0,0)]*len(pixels);paths=[];report=[]
    for index,color in enumerate(colors):
        mask=[p[3]>127 if index==0 else p==color for p in pixels]
        loops=[rounded_path(p) for p in contours(mask,width,height)]
        polygons=[points for _,points in loops]
        for y in range(height):
            crossings=intersections(y+.5,polygons,1)
            for left,right in zip(crossings[::2],crossings[1::2]):
                for x in range(max(0,math.ceil(left-.5)),min(width,math.ceil(right-.5))): reconstructed[y*width+x]=color
        hexcolor='#'+''.join(f'{c:02x}' for c in color[:3])
        paths.append(f'<path fill="{hexcolor}" fill-opacity="{color[3]/255:.6f}" fill-rule="evenodd" d="'+ ' '.join(d for d,_ in loops)+'"/>')
        report.append({'color':hexcolor,'contours':len(loops)});regions.append((color,polygons))
    mismatch=sum((p if p[3] else (0,0,0,0))!=q for p,q in zip(pixels,reconstructed))
    if mismatch: raise ValueError(f'Vector region registration failed: {mismatch} source pixels')
    large=size*2;canvas=bytearray(large*large*4);scale=large/width
    for color,polygons in regions:
        fill=bytes(color)
        for y in range(large):
            crossings=intersections(y+.5,polygons,scale)
            for left,right in zip(crossings[::2],crossings[1::2]):
                start=max(0,math.ceil(left-.5));end=min(large,math.ceil(right-.5))
                if end>start: canvas[(y*large+start)*4:(y*large+end)*4]=fill*(end-start)
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {width} {height}">\n'+'\n'.join(paths)+'\n</svg>\n'
    receipt={'source_size':[width,height],'output_size':size,'regions':report,'source_pixel_center_mismatches':mismatch,'contour_fairing_max_source_pixels':.35,'corner_rounding_max_source_pixels':.18,'method':'marching contour; bounded two-pass fairing; local quadratic rounding','raster':'stdlib even-odd scan conversion; alpha-premultiplied separable Lanczos-3','limitation':'Smooths source contours; adds no geographic detail. Pillow byte parity is not assumed.'}
    return lanczos_half(canvas,large),svg.encode(),json.dumps(receipt,sort_keys=True,indent=2).encode()+b'\n'
