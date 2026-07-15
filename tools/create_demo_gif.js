const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const root = path.resolve(__dirname, '..');
const outputDir = path.join(root, 'firmware', '1.0.0');
const frameDir = path.join(outputDir, '.demo-gif-frames');
const output = path.join(outputDir, 'civic-boost-gauge-demo.gif');
const captureSource = path.join(root, 'tools', 'prebaked_capture.bin');
const cacheSource = path.join(root, 'tools', 'prebaked_gauge_cache.bin');
const fontSource = path.join(root, 'src', 'boost_font_90_bold.c');

const width = 466;
const height = 466;
const pixelCount = width * height;
const minPsi = -15;
const maxPsi = 30;
const zeroAngleTenths = 450;
const sweepAngleTenths = 2700;
const valueLogicalX = 108;
const valueLogicalY = 176;
const valueLogicalWidth = 250;
const valueLineHeight = 66;
const valueBaseline = 1;
const frameCount = 601;
const frameDelay = 2;
const headerSize = 14 * 4;

function run(command, args) {
    const result = spawnSync(command, args, { stdio: 'inherit', shell: false });
    if (result.status !== 0) process.exit(result.status || 1);
}

function align4(value) {
    return (value + 3) & ~3;
}

function swap16(value) {
    return ((value & 0xff) << 8) | (value >>> 8);
}

function lvColorMix(foreground, background, opacity) {
    const fg16 = swap16(foreground);
    const bg16 = swap16(background);
    const mix = (opacity + 4) >>> 3;
    const mask = 0x07e0f81f;
    const bg = ((bg16 | (bg16 << 16)) >>> 0) & mask;
    const fg = ((fg16 | (fg16 << 16)) >>> 0) & mask;
    const product = Math.imul((fg - bg) >>> 0, mix) >>> 0;
    const result = (((product >>> 5) + bg) >>> 0) & mask;
    return swap16(((result >>> 16) | result) & 0xffff);
}

function writeRotatedPpm(file, pixels) {
    const header = Buffer.from(`P6\n${width} ${height}\n255\n`, 'ascii');
    const rgb = Buffer.allocUnsafe(pixelCount * 3);

    for (let y = 0; y < height; y += 1) {
        for (let x = 0; x < width; x += 1) {
            const value = swap16(pixels[y * width + x]);
            const finalX = y;
            const finalY = width - 1 - x;
            const destination = (finalY * width + finalX) * 3;
            rgb[destination] = ((value >>> 11) & 0x1f) * 255 / 31;
            rgb[destination + 1] = ((value >>> 5) & 0x3f) * 255 / 63;
            rgb[destination + 2] = (value & 0x1f) * 255 / 31;
        }
    }

    fs.writeFileSync(file, Buffer.concat([header, rgb]));
}

function parseCapturedGauge() {
    const capture = fs.readFileSync(captureSource);
    const marker = Buffer.from('PBK1 gauge ', 'ascii');
    const markerAt = capture.indexOf(marker);
    if (markerAt < 0) throw new Error('Captured gauge frame is missing');

    const countStart = markerAt + marker.length;
    const lineEnd = capture.indexOf(0x0a, countStart);
    const count = Number.parseInt(
        capture.subarray(countStart, lineEnd).toString('ascii').trim(), 10,
    );
    const payloadStart = lineEnd + 1;
    const pixels = new Uint16Array(pixelCount);

    for (let index = 0; index < count; index += 1) {
        const source = payloadStart + index * 6;
        const offset = capture.readUInt32LE(source);
        if (offset >= pixelCount) throw new Error('Captured gauge pixel is out of bounds');
        pixels[offset] = capture.readUInt16LE(source + 4);
    }
    return pixels;
}

function parseCache() {
    const data = fs.readFileSync(cacheSource);
    const fields = Array.from({ length: 14 }, (_, index) => data.readUInt32LE(index * 4));
    const [magic, version, totalSize, , stateCount, stateSize, tileMaskBytes,
        arcCommandCount, arcCommandSize, cursorPixelCount, cursorPixelSize,
        spatialRunCount, spatialRunSize, arcPixelCount] = fields;

    if (magic !== 0x31434742 || version !== 1 || totalSize !== data.length) {
        throw new Error('Unexpected prebaked gauge cache format');
    }
    if (stateSize !== 32 || arcCommandSize !== 4 ||
        cursorPixelSize !== 4 || spatialRunSize !== 8) {
        throw new Error('Prebaked gauge cache structures do not match the firmware');
    }

    let offset = headerSize;
    const statesOffset = align4(offset);
    offset = statesOffset + stateCount * stateSize;
    offset = align4(offset) + stateCount * tileMaskBytes;
    const arcCommandsOffset = align4(offset);
    offset = arcCommandsOffset + arcCommandCount * arcCommandSize;
    const cursorPixelsOffset = align4(offset);
    offset = cursorPixelsOffset + cursorPixelCount * cursorPixelSize;
    const spatialRunsOffset = align4(offset);
    offset = spatialRunsOffset + spatialRunCount * spatialRunSize;
    if (offset !== totalSize) throw new Error('Prebaked gauge cache has an invalid layout');

    const states = Array.from({ length: stateCount }, (_, index) => {
        const at = statesOffset + index * stateSize;
        return {
            firstArcCommand: data.readUInt32LE(at),
            firstCursorPixel: data.readUInt32LE(at + 4),
            arcCommandCount: data.readUInt16LE(at + 8),
            cursorPixelCount: data.readUInt16LE(at + 10),
            cursorArea: {
                x1: data.readInt16LE(at + 14),
                y1: data.readInt16LE(at + 16),
                x2: data.readInt16LE(at + 18),
                y2: data.readInt16LE(at + 20),
            },
        };
    });

    const spatialRuns = Array.from({ length: spatialRunCount }, (_, index) => {
        const at = spatialRunsOffset + index * spatialRunSize;
        return {
            offset: data.readUInt32LE(at),
            length: data.readUInt16LE(at + 6),
        };
    });

    if (spatialRuns.reduce((sum, run) => sum + run.length, 0) !== arcPixelCount) {
        throw new Error('Prebaked arc run length does not match its pixel count');
    }

    return {
        data,
        states,
        arcCommandsOffset,
        cursorPixelsOffset,
        spatialRuns,
    };
}

function parseFont() {
    const source = fs.readFileSync(fontSource, 'utf8');
    const bitmapBlock = source.match(/glyph_bitmap\[\] = \{([\s\S]*?)\n\};/);
    const descriptorBlock = source.match(/glyph_dsc\[\] = \{([\s\S]*?)\n\};/);
    if (!bitmapBlock || !descriptorBlock) throw new Error('Unable to parse the LVGL value font');

    const bitmap = Buffer.from([...bitmapBlock[1].matchAll(/0x([0-9a-f]+)/gi)]
        .map((match) => Number.parseInt(match[1], 16)));
    const descriptors = [...descriptorBlock[1].matchAll(
        /\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)/g,
    )].map((match) => ({
        bitmapIndex: Number(match[1]),
        advance: (Number(match[2]) + 8) >>> 4,
        boxWidth: Number(match[3]),
        boxHeight: Number(match[4]),
        offsetX: Number(match[5]),
        offsetY: Number(match[6]),
    })).filter((descriptor) => descriptor.advance > 0);

    const characters = '-.0123456789';
    if (descriptors.length !== characters.length) {
        throw new Error('Unexpected LVGL value font glyph count');
    }

    return Object.fromEntries(characters.split('').map((character, index) => {
        const descriptor = descriptors[index];
        const glyphWidth = descriptor.boxHeight;
        const glyphHeight = descriptor.boxWidth;
        const opacity = new Uint8Array(glyphWidth * glyphHeight);

        for (let row = 0; row < descriptor.boxHeight; row += 1) {
            for (let column = 0; column < descriptor.boxWidth; column += 1) {
                const sourcePixel = row * descriptor.boxWidth + column;
                const packed = bitmap[descriptor.bitmapIndex + (sourcePixel >>> 1)];
                const shade = sourcePixel & 1 ? packed & 0x0f : packed >>> 4;
                const rotatedX = descriptor.boxHeight - 1 - row;
                const rotatedY = column;
                opacity[rotatedY * glyphWidth + rotatedX] = shade * 17;
            }
        }

        return [character, { ...descriptor, glyphWidth, glyphHeight, opacity }];
    }));
}

function applyArcState(pixels, cache, state) {
    let spatialRun = 0;
    let usedInRun = 0;

    for (let index = 0; index < state.arcCommandCount; index += 1) {
        const at = cache.arcCommandsOffset + (state.firstArcCommand + index) * 4;
        let remaining = cache.data.readUInt16LE(at);
        const color = cache.data.readUInt16LE(at + 2);

        while (remaining !== 0) {
            const run = cache.spatialRuns[spatialRun];
            const chunk = Math.min(remaining, run.length - usedInRun);
            pixels.fill(color, run.offset + usedInRun, run.offset + usedInRun + chunk);
            remaining -= chunk;
            usedInRun += chunk;
            if (usedInRun === run.length) {
                spatialRun += 1;
                usedInRun = 0;
            }
        }
    }
}

function applyCursorState(pixels, cache, state) {
    const cursorWidth = state.cursorArea.x2 - state.cursorArea.x1 + 1;
    for (let index = 0; index < state.cursorPixelCount; index += 1) {
        const at = cache.cursorPixelsOffset + (state.firstCursorPixel + index) * 4;
        const relativeOffset = cache.data.readUInt16LE(at);
        const relativeY = Math.floor(relativeOffset / cursorWidth);
        const relativeX = relativeOffset - relativeY * cursorWidth;
        const x = state.cursorArea.x1 + relativeX;
        const y = state.cursorArea.y1 + relativeY;
        pixels[y * width + x] = cache.data.readUInt16LE(at + 2);
    }
}

function applyValue(pixels, glyphs, value) {
    const normalized = Math.abs(value) < 0.0001 ? 0 : value;
    const text = normalized.toFixed(1);
    const textWidth = [...text].reduce((sum, character) => sum + glyphs[character].advance, 0);
    let logicalX = valueLogicalX + Math.trunc((valueLogicalWidth - textWidth) / 2);

    for (const character of text) {
        const glyph = glyphs[character];
        const glyphX = logicalX + glyph.offsetX;
        const glyphY = valueLogicalY + (valueLineHeight - valueBaseline) -
            glyph.boxHeight - glyph.offsetY;
        const physicalX = height - glyphY - glyph.boxHeight;
        const physicalY = glyphX;

        for (let y = 0; y < glyph.glyphHeight; y += 1) {
            for (let x = 0; x < glyph.glyphWidth; x += 1) {
                const opacity = glyph.opacity[y * glyph.glyphWidth + x];
                if (opacity === 0) continue;
                const destinationX = physicalX + x;
                const destinationY = physicalY + y;
                if (destinationX < 0 || destinationX >= width ||
                    destinationY < 0 || destinationY >= height) continue;
                const destination = destinationY * width + destinationX;
                pixels[destination] = opacity === 255
                    ? 0xffff
                    : lvColorMix(0xffff, pixels[destination], opacity);
            }
        }
        logicalX += glyph.advance;
    }
}

function psiForAngleTenths(angleTenths) {
    if (angleTenths <= zeroAngleTenths) {
        return minPsi + (angleTenths / zeroAngleTenths) * -minPsi;
    }
    return ((angleTenths - zeroAngleTenths) /
        (sweepAngleTenths - zeroAngleTenths)) * maxPsi;
}

fs.rmSync(frameDir, { recursive: true, force: true });
fs.mkdirSync(frameDir, { recursive: true });

const staticFrame = parseCapturedGauge();
const cache = parseCache();
const glyphs = parseFont();

for (let index = 0; index < frameCount; index += 1) {
    const phase = index / (frameCount - 1);
    const triangle = phase <= 0.5 ? phase * 2 : (1 - phase) * 2;
    const stateIndex = Math.round(triangle * (cache.states.length - 1));
    const angleTenths = stateIndex * 5;
    const pixels = staticFrame.slice();
    const state = cache.states[stateIndex];

    applyArcState(pixels, cache, state);
    applyCursorState(pixels, cache, state);
    applyValue(pixels, glyphs, psiForAngleTenths(angleTenths));
    writeRotatedPpm(
        path.join(frameDir, `frame-${String(index).padStart(3, '0')}.ppm`),
        pixels,
    );
}

run('magick', [
    '-background', 'black',
    path.join(frameDir, 'frame-*.ppm'),
    '-set', 'delay', String(frameDelay),
    '-loop', '0',
    '-layers', 'Optimize',
    output,
]);

fs.rmSync(frameDir, { recursive: true, force: true });
