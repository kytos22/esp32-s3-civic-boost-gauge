const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const root = path.resolve(__dirname, '..');
const outputDir = path.join(root, 'firmware', '1.0.0');
const frameDir = path.join(outputDir, '.demo-gif-frames');
const rotatedBase = path.join(frameDir, 'gauge-static.png');
const output = path.join(outputDir, 'civic-boost-gauge-demo.gif');
const fontSource = path.join(root, 'src', 'boost_font_90_bold.c');
const staticGauge = path.join(root, 'tools', 'prebaked_gauge.png');

// These values intentionally mirror src/main.cpp. The static layer comes from
// the captured prebaked frame and the dynamic layer uses the same cache geometry.
const width = 466;
const height = 466;
const centerX = 232;
const centerY = 233;
const startDeg = 135;
const endDeg = 405;
const minPsi = -15;
const maxPsi = 30;
const arcRadius = 221;
const arcWidth = 24;
const cursorInnerRadius = 199;
const cursorOuterRadius = 232;
const valueLogicalX = 108;
const valueLogicalY = 176;
const valueLogicalWidth = 250;
const valueLineHeight = 66;
const valueBaseline = 1;
const frameCount = 301;
const frameDelay = 4;
const cursorColor = '#f44336';

function run(command, args) {
    const result = spawnSync(command, args, { stdio: 'inherit', shell: false });
    if (result.status !== 0) process.exit(result.status || 1);
}

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

function smoothstep(value) {
    const clamped = Math.max(0, Math.min(1, value));
    return clamped * clamped * (3 - 2 * clamped);
}

function mix(left, right, amount) {
    return left.map((value, index) => Math.round(value + (right[index] - value) * amount));
}

function rgb(color) {
    return `rgb(${color.join(',')})`;
}

function boostColor(psi) {
    const blue = [33, 150, 243];
    const green = [76, 175, 80];
    const yellow = [255, 235, 59];
    const red = [244, 67, 54];

    if (psi < -2) return rgb(blue);
    if (psi < 2) return rgb(mix(blue, green, smoothstep((psi + 2) / 4)));
    if (psi < 12) return rgb(green);
    if (psi < 15) return rgb(mix(green, yellow, smoothstep((psi - 12) / 3)));
    if (psi < 20) return rgb(yellow);
    if (psi < 26) return rgb(mix(yellow, red, smoothstep((psi - 20) / 6)));
    return rgb(red);
}

function parseFont() {
    const source = fs.readFileSync(fontSource, 'utf8');
    const bitmapBlock = source.match(/glyph_bitmap\[\] = \{([\s\S]*?)\n\};/);
    const descriptorBlock = source.match(/glyph_dsc\[\] = \{([\s\S]*?)\n\};/);
    if (!bitmapBlock || !descriptorBlock) throw new Error('Unable to parse the LVGL value font');

    const bitmap = [...bitmapBlock[1].matchAll(/0x([0-9a-f]+)/gi)]
        .map((match) => Number.parseInt(match[1], 16));
    const descriptors = [...descriptorBlock[1].matchAll(
        /\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)/g,
    )].map((match) => ({
        bitmapIndex: Number(match[1]),
        advance: Math.floor(Number(match[2]) / 16),
        width: Number(match[3]),
        height: Number(match[4]),
        offsetX: Number(match[5]),
        offsetY: Number(match[6]),
    })).filter((descriptor) => descriptor.advance > 0);

    const characters = '-.0123456789';
    return Object.fromEntries(characters.split('').map((character, index) => [
        character,
        { ...descriptors[index], bitmap },
    ]));
}

const glyphs = parseFont();

function valuePixels(value) {
    const text = value.toFixed(1);
    const textWidth = [...text].reduce((sum, character) => sum + glyphs[character].advance, 0);
    let logicalX = valueLogicalX + Math.floor((valueLogicalWidth - textWidth) / 2);
    const textStartX = logicalX;
    const pixels = [];

    for (const character of text) {
        const glyph = glyphs[character];
        const glyphX = logicalX + glyph.offsetX;
        const glyphY = valueLogicalY + (valueLineHeight - valueBaseline) - glyph.height - glyph.offsetY;
        for (let row = 0; row < glyph.height; row += 1) {
            for (let column = 0; column < glyph.width; column += 1) {
                const sourcePixel = row * glyph.width + column;
                const packed = glyph.bitmap[glyph.bitmapIndex + (sourcePixel >> 1)];
                const shade = sourcePixel & 1 ? packed & 0x0f : packed >> 4;
                if (shade === 0) continue;
                pixels.push(`<rect x="${glyphX + column}" y="${glyphY + row}" width="1" height="1" fill="#fff" fill-opacity="${(shade / 15).toFixed(3)}"/>`);
            }
        }
        logicalX += glyph.advance;
    }
    // ImageMagick drops the tiny alpha-only LVGL minus glyph in some SVG
    // frames. Preserve its exact 25x13 bounding box for negative pressures.
    const negativeSign = value < 0
        ? `<rect x="${textStartX + 6}" y="${valueLogicalY + 35}" width="25" height="13" fill="#fff"/>`
        : '';
    return `${negativeSign}${pixels.join('')}`;
}

function makeFrame(index) {
    const phase = index / (frameCount - 1);
    const triangle = phase <= 0.5 ? phase * 2 : (1 - phase) * 2;
    // Keep the value, cursor angle and revealed arc in lockstep. The GIF is a
    // visual demo, so its sweep is deliberately linear in both directions.
    const psi = minPsi + triangle * (maxPsi - minPsi);
    const progress = (psi - minPsi) / (maxPsi - minPsi);
    const currentDeg = startDeg + progress * (endDeg - startDeg);
    const cursorInner = polar(currentDeg, cursorInnerRadius);
    const cursorOuter = polar(currentDeg, cursorOuterRadius);
    const color = boostColor(psi);
    const arc = currentDeg > startDeg
        ? `<path d="${pathForArc(startDeg, currentDeg, arcRadius)}" fill="none" stroke="${color}" stroke-width="${arcWidth}"/>`
        : '';

    return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">
  ${arc}
  <line x1="${cursorInner.x.toFixed(2)}" y1="${cursorInner.y.toFixed(2)}" x2="${cursorOuter.x.toFixed(2)}" y2="${cursorOuter.y.toFixed(2)}" stroke="${cursorColor}" stroke-width="8" stroke-linecap="round"/>
  ${valuePixels(psi)}
</svg>`;
}

fs.rmSync(frameDir, { recursive: true, force: true });
fs.mkdirSync(frameDir, { recursive: true });

// The captured prebaked visual is in the controller-native orientation. The
// physical UI rotates that layout counter-clockwise, exactly as the firmware's
// rotated glyph path does.
run('magick', [staticGauge, '-rotate', '-90', rotatedBase]);
for (let index = 0; index < frameCount; index += 1) {
    const frameBase = path.join(frameDir, `frame-${String(index).padStart(3, '0')}`);
    const svgPath = `${frameBase}.svg`;
    const pngPath = `${frameBase}.png`;
    fs.writeFileSync(svgPath, makeFrame(index));
    run('magick', [
        rotatedBase,
        '-background', 'none', svgPath,
        '-compose', 'over', '-composite', pngPath,
    ]);
}

run('magick', [
    '-background', 'black',
    '-density', '144',
    path.join(frameDir, 'frame-*.png'),
    '-resize', '466x466',
    '-set', 'delay', String(frameDelay),
    '-loop', '0',
    '-layers', 'Optimize',
    output,
]);

fs.rmSync(frameDir, { recursive: true, force: true });
