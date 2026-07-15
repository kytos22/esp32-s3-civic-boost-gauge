const fs = require("fs");

const [inputPath, widthText, heightText, outputPath, symbol] = process.argv.slice(2);
const width = Number(widthText);
const height = Number(heightText);
const rgba = fs.readFileSync(inputPath);

if (!inputPath || !outputPath || !symbol || rgba.length !== width * height * 4) {
  throw new Error("Usage: node generate_lvgl_image.js input.rgba width height output.c symbol");
}

const bytes = [];
for (let i = 0; i < rgba.length; i += 4) {
  const color565 = ((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 2) << 5) | (rgba[i + 2] >> 3);
  bytes.push(color565 & 0xff, color565 >> 8, rgba[i + 3]);
}

const rows = [];
for (let i = 0; i < bytes.length; i += 12) {
  rows.push("    " + bytes.slice(i, i + 12).map((byte) => `0x${byte.toString(16).padStart(2, "0")}`).join(", "));
}

const source = `#include <lvgl.h>\n\nstatic const uint8_t ${symbol}_map[] = {\n${rows.join(",\n")}\n};\n\nconst lv_img_dsc_t ${symbol} = {\n    .header = {\n        .always_zero = 0,\n        .w = ${width},\n        .h = ${height},\n        .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n    },\n    .data_size = sizeof(${symbol}_map),\n    .data = ${symbol}_map,\n};\n`;

fs.writeFileSync(outputPath, source);
