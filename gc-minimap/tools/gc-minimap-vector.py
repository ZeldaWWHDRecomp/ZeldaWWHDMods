#!/usr/bin/env python3
"""Trace user-owned GC map regions; write derived assets outside Git only.

Requires Pillow and numpy. No learned reconstruction: preserve
the source canvas and palette; constrain smoothing in source-pixel units.
"""
import argparse
import json
import struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageChops


def contours(mask):
    # Marching squares over padded pixel-center samples. Ambiguous diagonal
    # cases connect corner-touching land, preserving diagonal piers. Separate
    # islands with an actual sea-pixel gap remain disconnected.
    grid = np.pad(mask, 1)
    pairs = {1:[(0,3)], 2:[(0,1)], 3:[(3,1)], 4:[(1,2)],
             5:[(0,1),(2,3)], 6:[(0,2)], 7:[(3,2)], 8:[(2,3)],
             9:[(0,2)], 10:[(0,3),(1,2)], 11:[(1,2)],
             12:[(3,1)], 13:[(0,1)], 14:[(0,3)]}
    graph = {}
    for y in range(grid.shape[0]-1):
        for x in range(grid.shape[1]-1):
            case = int(grid[y,x])+2*int(grid[y,x+1])+4*int(grid[y+1,x+1])+8*int(grid[y+1,x])
            pts = [(2*x+1,2*y),(2*x+2,2*y+1),(2*x+1,2*y+2),(2*x,2*y+1)]
            for ia, ib in pairs.get(case, []):
                a,b = pts[ia],pts[ib]
                graph.setdefault(a,[]).append(b)
                graph.setdefault(b,[]).append(a)
    result = []
    while graph:
        start = next(iter(graph));current=start;previous=None;points=[]
        while True:
            points.append(current)
            neighbors=graph[current]
            following=next(v for v in neighbors if v!=previous)
            previous,current=current,following
            if current==start:
                break
        for point in points:
            del graph[point]
        p = np.array(points,dtype=float)/2-.5
        original = p.copy()
        for _ in range(2):
            p = .25*np.roll(p,1,axis=0)+.5*p+.25*np.roll(p,-1,axis=0)
        delta=p-original
        length=np.linalg.norm(delta,axis=1)
        p=original+delta*np.minimum(1,.35/np.maximum(length,1e-9))[:,None]
        keep = []
        for i, point in enumerate(p):
            a, b = point-p[i-1], p[(i+1) % len(p)]-point
            if abs(a[0]*b[1]-a[1]*b[0]) > 1e-8:
                keep.append(point)
        if len(keep) >= 3:
            result.append(np.array(keep))
    return result


def points_in_poly(points, polygon):
    inside=np.zeros(len(points),dtype=bool)
    x,y=points[:,0],points[:,1]
    for a,b in zip(polygon,np.roll(polygon,-1,axis=0)):
        if a[1]==b[1]:
            continue
        crosses=(a[1]>y)!=(b[1]>y)
        inside ^= crosses & (x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0])
    return inside


def rounded_path(p):
    before, after = [], []
    for i, v in enumerate(p):
        a, b = p[i-1]-v, p[(i+1) % len(p)]-v
        la, lb = np.linalg.norm(a), np.linalg.norm(b)
        radius = min(.18, la*.25, lb*.25)
        before.append(v+a/ la*radius)
        after.append(v+b/ lb*radius)
    f = lambda v: f'{v[0]:.5f},{v[1]:.5f}'
    commands = ['M'+f(after[-1])]
    # The raster preview samples the same quadratic curves as the SVG.
    samples = [after[-1]]
    for v, a, b in zip(p, before, after):
        commands += ['L'+f(a), 'Q'+f(v)+' '+f(b)]
        samples.append(a)
        for t in np.linspace(0, 1, 9)[1:]:
            samples.append((1-t)**2*a+2*(1-t)*t*v+t*t*b)
    return ' '.join(commands)+' Z', np.array(samples)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--size', type=int, default=1024)
    args = parser.parse_args()
    output = args.output.resolve()
    if any((parent/'.git').exists() for parent in (output, *output.parents)):
        raise SystemExit('Derived game assets must remain outside Git')
    if not 120 <= args.size <= 4096:
        raise SystemExit('Preview size must be between 120 and 4096')
    output.mkdir(parents=True, exist_ok=True)
    rgba = np.array(Image.open(args.input).convert('RGBA'))
    h, w = rgba.shape[:2]
    colors, counts = np.unique(rgba[rgba[:, :, 3] > 127], axis=0,
                              return_counts=True)
    if not len(colors) or len(colors) > 32:
        raise SystemExit('Expected a small indexed GC palette')
    # Light terrain fills the land silhouette; dark ink overlays it so
    # diagonal internal paths stay continuous as well as the coastline.
    order = np.argsort(-colors[:,:3].astype(float).dot([.2126,.7152,.0722]))
    colors = colors[order]
    scale = args.size / w
    if h != w:
        raise SystemExit('Expected a square map')
    ss = 2
    canvas = Image.new('RGBA', (args.size*ss, args.size*ss))
    yy, xx = np.mgrid[:h, :w]
    centers = np.column_stack((xx.ravel()+.5, yy.ravel()+.5))
    reconstructed = np.zeros_like(rgba)
    paths, report = [], []
    for i, color in enumerate(colors):
        mask = rgba[:, :, 3] > 127 if i == 0 else np.all(rgba == color, axis=2)
        loops = [rounded_path(p) for p in contours(mask)]
        raster_mask = Image.new('1', canvas.size)
        center_mask = np.zeros(w*h, dtype=bool)
        for _, points in loops:
            region = Image.new('1', canvas.size)
            ImageDraw.Draw(region).polygon([tuple(v*scale*ss) for v in points], fill=1)
            raster_mask = ImageChops.logical_xor(raster_mask, region)
            center_mask ^= points_in_poly(centers, points)
        fill = Image.new('RGBA', canvas.size, tuple(map(int, color)))
        canvas.paste(fill, (0, 0), raster_mask.convert('L'))
        reconstructed.reshape(-1, 4)[center_mask] = color
        hexcolor = '#'+''.join(f'{int(c):02x}' for c in color[:3])
        paths.append(f'<path fill="{hexcolor}" fill-opacity="{color[3]/255:.6f}" fill-rule="evenodd" d="'+ ' '.join(d for d, _ in loops)+'"/>')
        report.append({'color': hexcolor, 'contours': len(loops)})
    # Ignore RGB underneath transparent pixels; retain every visible label.
    expected = rgba.copy()
    expected[expected[:, :, 3] == 0] = 0
    mismatch = int(np.count_nonzero(np.any(expected != reconstructed, axis=2)))
    if mismatch:
        raise SystemExit(f'Vector region registration failed: {mismatch} source pixels')
    stem = args.input.stem.replace('-gc-original', '').replace('-original', '')+'-vector'
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{args.size}" height="{args.size}" viewBox="0 0 {w} {h}">\n'+'\n'.join(paths)+'\n</svg>\n'
    (output/(stem+'.svg')).write_text(svg)
    raster = canvas.resize((args.size, args.size), Image.Resampling.LANCZOS)
    raster.save(output/(stem+'.png'))
    (output/(stem.removesuffix('-vector')+'.vector-rgba')).write_bytes(b'GCM1'+struct.pack('>II',args.size,args.size)+raster.tobytes())
    receipt = {'source': str(args.input), 'source_size': [w, h],
               'output_size': args.size, 'regions': report,
               'source_pixel_center_mismatches': mismatch,
               'contour_fairing_max_source_pixels': .35,
               'corner_rounding_max_source_pixels': .18,
               'method': 'marching contour; bounded two-pass fairing; local quadratic rounding',
               'limitation': 'Smooths source contours; adds no geographic detail.'}
    (output/(stem+'.json')).write_text(json.dumps(receipt, indent=2)+'\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
