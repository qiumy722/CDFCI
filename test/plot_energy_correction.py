"""Create publication-style CDFCI error curves without matplotlib."""

import csv
import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import landscape, letter
from reportlab.pdfgen import canvas


def load_font(size, bold=False):
    names = ["arialbd.ttf" if bold else "arial.ttf",
             "timesbd.ttf" if bold else "times.ttf"]
    for name in names:
        path = Path("C:/Windows/Fonts") / name
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def draw_centered(draw, x, y, text, text_font, fill):
    box = draw.textbbox((0, 0), text, font=text_font)
    draw.text((x - (box[2] - box[0]) / 2, y - (box[3] - box[1]) / 2),
              text, font=text_font, fill=fill)


def marker(draw, x, y, shape, color, radius=5):
    if shape == "circle":
        draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=color)
    elif shape == "square":
        draw.rectangle((x-radius, y-radius, x+radius, y+radius), fill=color)
    else:
        draw.polygon([(x, y-radius-1), (x-radius-1, y+radius),
                      (x+radius+1, y+radius)], fill=color)


def main(input_name, output_dir, stem="h2o_100k_energy_errors"):
    source = Path(input_name)
    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)
    data = json.loads(source.read_text(encoding="utf-8"))
    reference = float(data["reference_energy"])
    rows = data["trajectory"]
    if not rows or any(row["status"] != "ok" for row in rows):
        raise RuntimeError("Trajectory is empty or contains invalid correction points")

    series = [
        ("E", "E", (64, 92, 132), "circle"),
        ("E + E_2^(2)", "E_plus_E2_2", (221, 132, 48), "square"),
        ("E + E_2^(1) + E_2^(2)", "E_plus_E2_1_plus_E2_2", (28, 143, 130), "triangle"),
    ]
    processed = []
    for row in rows:
        e = float(row["variational_energy"])
        external = float(row["external_correction"])
        internal = float(row["internal_correction"])
        values = {"E": e,
                  "E_plus_E2_2": e + external,
                  "E_plus_E2_1_plus_E2_2": e + external + internal}
        errors = {key: abs(value - reference) for key, value in values.items()}
        if any(value <= 0 or not math.isfinite(value) for value in errors.values()):
            raise RuntimeError("Log plot requires finite, positive errors")
        processed.append({"iteration": int(row["iteration"]), "values": values,
                          "errors": errors})

    csv_path = out / f"{stem}.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["iteration", "E", "E_plus_E2_2",
                         "E_plus_E2_1_plus_E2_2", "abs_error_E",
                         "abs_error_E_plus_E2_2",
                         "abs_error_E_plus_E2_1_plus_E2_2"])
        for row in processed:
            writer.writerow([row["iteration"],
                             f"{row['values']['E']:.16g}",
                             f"{row['values']['E_plus_E2_2']:.16g}",
                             f"{row['values']['E_plus_E2_1_plus_E2_2']:.16g}",
                             f"{row['errors']['E']:.16g}",
                             f"{row['errors']['E_plus_E2_2']:.16g}",
                             f"{row['errors']['E_plus_E2_1_plus_E2_2']:.16g}"])

    all_errors = [row["errors"][key] for row in processed for _, key, _, _ in series]
    log_min = math.floor(math.log10(min(all_errors)))
    log_max = math.ceil(math.log10(max(all_errors)))
    if log_min == log_max:
        log_min -= 1
    max_iteration = max(row["iteration"] for row in processed)

    width, height = 2000, 1250
    left, right, top, bottom = 230, 1920, 190, 1055
    plot_width, plot_height = right-left, bottom-top
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    title_font = load_font(54, True)
    subtitle_font = load_font(31)
    axis_font = load_font(34)
    tick_font = load_font(29)
    legend_font = load_font(31)
    colors = {"text": (31, 38, 47), "muted": (91, 101, 113),
              "major": (202, 208, 215), "minor": (231, 234, 238)}

    def map_x(iteration):
        return left + iteration / max_iteration * plot_width

    def map_y(value):
        return top + (log_max - math.log10(value)) / (log_max-log_min) * plot_height

    draw_centered(draw, width/2, 65,
                  "CDFCI energy error along a 100,000-step serial trajectory",
                  title_font, colors["text"])
    z_threshold = float(data.get("options", {}).get("z_threshold", 0.0))
    threshold_label = "0" if z_threshold == 0 else f"{z_threshold:.0e}"
    draw_centered(draw, width/2, 125,
                  f"H2O FCIDUMP: 12 spatial orbitals, 8 electrons; z-threshold = {threshold_label}",
                  subtitle_font, colors["muted"])

    for exponent in range(log_min, log_max+1):
        y = map_y(10.0**exponent)
        draw.line((left, y, right, y), fill=colors["major"], width=2)
        draw_centered(draw, left-70, y, f"10^{exponent}", tick_font, colors["text"])
        if exponent < log_max:
            for factor in (2, 5):
                ym = map_y(factor * 10.0**exponent)
                draw.line((left, ym, right, ym), fill=colors["minor"], width=1)

    for iteration in range(0, max_iteration+1, 20000):
        x = map_x(iteration)
        draw.line((x, top, x, bottom), fill=colors["minor"], width=1)
        label = "0" if iteration == 0 else f"{iteration//1000}k"
        draw_centered(draw, x, bottom+44, label, tick_font, colors["text"])

    draw.line((left, top, left, bottom), fill=colors["text"], width=3)
    draw.line((left, bottom, right, bottom), fill=colors["text"], width=3)
    draw_centered(draw, (left+right)/2, 1145, "CDFCI iteration", axis_font, colors["text"])
    y_label = Image.new("RGBA", (900, 70), (255, 255, 255, 0))
    y_draw = ImageDraw.Draw(y_label)
    draw_centered(y_draw, 450, 35, "Absolute energy error |E_est - E_FCI| (Ha)",
                  axis_font, colors["text"])
    y_label = y_label.rotate(90, expand=True)
    image.paste(y_label, (55, int((height-y_label.height)/2)), y_label)

    for label, key, color, shape in series:
        points = [(map_x(row["iteration"]), map_y(row["errors"][key]))
                  for row in processed]
        draw.line(points, fill=color, width=5, joint="curve")
        for x, y in points:
            marker(draw, x, y, shape, color, radius=5)

    legend_x, legend_y = 1120, 225
    for index, (label, _, color, shape) in enumerate(series):
        y = legend_y + index*56
        draw.line((legend_x, y, legend_x+74, y), fill=color, width=5)
        marker(draw, legend_x+37, y, shape, color, radius=7)
        draw.text((legend_x+95, y-19), label, font=legend_font, fill=colors["text"])

    note = f"Reference E_FCI = {reference:.14f} Ha; markers every 1,000 iterations"
    draw.text((left, 1200), note, font=subtitle_font, fill=colors["muted"])

    png_path = out / f"{stem}.png"
    image.save(png_path, dpi=(300, 300))
    pdf_path = out / f"{stem}.pdf"
    page_width, page_height = landscape(letter)
    pdf = canvas.Canvas(str(pdf_path), pagesize=(page_width, page_height))
    pdf.drawImage(str(png_path), 0, 0, width=page_width, height=page_height,
                  preserveAspectRatio=True, anchor="c")
    pdf.showPage()
    pdf.save()

    summary = {"input": str(source), "reference_energy": reference,
               "z_threshold": z_threshold,
               "points": len(processed), "first_iteration": processed[0]["iteration"],
               "last_iteration": processed[-1]["iteration"], "sampling_interval": 1000,
               "series": {}}
    for label, key, _, _ in series:
        best = min(processed, key=lambda row: row["errors"][key])
        summary["series"][label] = {
            "first_error": processed[0]["errors"][key],
            "last_error": processed[-1]["errors"][key],
            "minimum_error": best["errors"][key],
            "minimum_error_iteration": best["iteration"],
        }
    summary_path = out / f"{stem}_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False)+"\n",
                            encoding="utf-8")
    print(json.dumps(summary, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        raise SystemExit("Usage: plot_energy_correction.py trajectory.json output_dir [output_stem]")
    main(sys.argv[1], sys.argv[2],
         sys.argv[3] if len(sys.argv) == 4 else "h2o_100k_energy_errors")
