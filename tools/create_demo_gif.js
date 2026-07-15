const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const root = path.resolve(__dirname, '..');
const outputDir = path.join(root, 'firmware', 'golden');
const frameDir = path.join(outputDir, '.demo-gif-frames');
const output = path.join(outputDir, 'civic-boost-gauge-demo.gif');

const width = 466;
const height = 466;
const centerX = 232;
const centerY = 233;
const startDeg = 225;
const endDeg = 495;
const minPsi = -15;
const maxPsi = 30;
const frameCount = 36;
const radius = 193;

function polar(degrees, distance) {
    const radians = degrees * Math.PI / 180;
    return {
        x: centerX + Math.cos(radians) * distance,
        y: centerY + Math.sin(radians) * distance,
    };
}

function pathForArc(start, end, distance) {
    const a = polar(start, distance);
    const b = polar(end, distance);
    const largeArc = end - start > 180 ? 1 : 0;
    return `M ${a.x.toFixed(2)} ${a.y.toFixed(2)} A ${distance} ${distance} 0 ${largeArc} 1 ${b.x.toFixed(2)} ${b.y.toFixed(2)}`;
}

function colorForProgress(progress) {
    const stops = [
        [0.00, [44, 151, 255]],
        [0.38, [58, 210, 112]],
        [0.67, [255, 225, 40]],
        [1.00, [255, 54, 45]],
    ];
    for (let i = 1; i < stops.length; i += 1) {
        if (progress <= stops[i][0]) {
            const [leftStop, rightStop] = [stops[i - 1], stops[i]];
            const ratio = (progress - leftStop[0]) / (rightStop[0] - leftStop[0]);
            const rgb = leftStop[1].map((value, index) =>
                Math.round(value + (rightStop[1][index] - value) * ratio));
            return `rgb(${rgb.join(',')})`;
        }
    }
    return 'rgb(255,54,45)';
}

function text(value, x, y, size, color = '#ffffff', anchor = 'middle') {
    return `<text x="${x}" y="${y}" fill="${color}" font-family="Arial, sans-serif" font-size="${size}px" font-weight="700" text-anchor="${anchor}">${value}</text>`;
}

function makeFrame(index) {
    const phase = index / (frameCount - 1);
    const triangle = phase <= 0.5 ? phase * 2 : (1 - phase) * 2;
    const smooth = triangle * triangle * (3 - 2 * triangle);
    const psi = minPsi + smooth * (maxPsi - minPsi);
    const progress = (psi - minPsi) / (maxPsi - minPsi);
    const currentDeg = startDeg + progress * (endDeg - startDeg);
    const cursorInner = polar(currentDeg, 177);
    const cursorOuter = polar(currentDeg, 222);
    const activeEnd = Math.max(startDeg + 1, currentDeg);
    const parts = [];

    parts.push('<rect width="466" height="466" fill="#000000"/>');
    parts.push(`<path d="${pathForArc(startDeg, endDeg, radius)}" fill="none" stroke="#262626" stroke-width="24"/>`);

    for (let angle = startDeg; angle < endDeg; angle += 5) {
        const tickProgress = (angle - startDeg) / (endDeg - startDeg);
        const major = Math.round(angle - startDeg) % 25 === 0;
        const inner = major ? 171 : 181;
        const outer = 201;
        const a = polar(angle, inner);
        const b = polar(angle, outer);
        const visible = angle <= currentDeg;
        parts.push(`<line x1="${a.x.toFixed(1)}" y1="${a.y.toFixed(1)}" x2="${b.x.toFixed(1)}" y2="${b.y.toFixed(1)}" stroke="${visible ? colorForProgress(tickProgress) : '#666666'}" stroke-width="${major ? 5 : 2}" stroke-linecap="round"/>`);
    }

    for (let psiLabel = -15; psiLabel <= 30; psiLabel += 5) {
        const labelProgress = (psiLabel - minPsi) / (maxPsi - minPsi);
        const labelAngle = startDeg + labelProgress * (endDeg - startDeg);
        const label = polar(labelAngle, 145);
        const labelColor = labelProgress <= progress ? colorForProgress(labelProgress) : '#a0a0a0';
        parts.push(text(psiLabel, label.x.toFixed(1), (label.y + 6).toFixed(1), 18, labelColor));
    }

    if (currentDeg > startDeg) {
        parts.push(`<path d="${pathForArc(startDeg, activeEnd, radius)}" fill="none" stroke="${colorForProgress(progress)}" stroke-width="24" stroke-linecap="round"/>`);
    }
    parts.push(`<line x1="${cursorInner.x.toFixed(1)}" y1="${cursorInner.y.toFixed(1)}" x2="${cursorOuter.x.toFixed(1)}" y2="${cursorOuter.y.toFixed(1)}" stroke="${colorForProgress(progress)}" stroke-width="14" stroke-linecap="round"/>`);
    parts.push(text(Math.round(psi), centerX, centerY + 25, 92));
    parts.push(text('PSI', centerX, centerY + 62, 25, '#ffffff'));
    parts.push(text('CIVIC', centerX, centerY + 126, 24, '#bfc4ca'));

    return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">${parts.join('')}</svg>`;
}

fs.rmSync(frameDir, { recursive: true, force: true });
fs.mkdirSync(frameDir, { recursive: true });
const framePaths = [];
for (let index = 0; index < frameCount; index += 1) {
    const framePath = path.join(frameDir, `frame-${String(index).padStart(2, '0')}.svg`);
    fs.writeFileSync(framePath, makeFrame(index));
    framePaths.push(framePath);
}

const result = spawnSync('magick', [
    '-background', 'black',
    '-density', '144',
    ...framePaths,
    '-resize', '466x466',
    '-set', 'delay', '8',
    '-loop', '0',
    output,
], { stdio: 'inherit', shell: false });

fs.rmSync(frameDir, { recursive: true, force: true });
if (result.status !== 0) process.exit(result.status || 1);
