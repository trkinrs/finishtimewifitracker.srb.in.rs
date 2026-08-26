#include "../shared/PeakDetector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct ReplayPoint {
  uint32_t timeMs;
  double seconds;
  int rawRssi;
  float smoothRssi;
  bool peak;
};

static std::vector<std::string> splitCsvLine(const std::string &line) {
  std::vector<std::string> cells;
  std::string cell;
  bool quoted = false;

  for (char c : line) {
    if (c == '"') {
      quoted = !quoted;
    } else if (c == ',' && !quoted) {
      cells.push_back(cell);
      cell.clear();
    } else {
      cell += c;
    }
  }
  cells.push_back(cell);
  return cells;
}

static int columnIndex(const std::vector<std::string> &header, const std::string &name) {
  for (size_t i = 0; i < header.size(); i++) {
    if (header[i] == name) {
      return (int)i;
    }
  }
  return -1;
}

static std::string xmlEscape(const std::string &text) {
  std::string escaped;
  for (char c : text) {
    if (c == '&') escaped += "&amp;";
    else if (c == '<') escaped += "&lt;";
    else if (c == '>') escaped += "&gt;";
    else if (c == '"') escaped += "&quot;";
    else escaped += c;
  }
  return escaped;
}

static double signalLevel(double rssi) {
  double clamped = std::max(-100.0, std::min(-35.0, rssi));
  return std::max(0.0, std::min(99.0, std::round(((clamped + 100.0) * 99.0) / 65.0)));
}

static double mapValue(double value, double inMin, double inMax, double outMin, double outMax) {
  return outMin + ((value - inMin) * (outMax - outMin)) / (inMax - inMin);
}

static void writePolyline(std::ostream &out,
                          const std::vector<ReplayPoint> &points,
                          bool smooth,
                          const char *color,
                          double minT,
                          double maxT,
                          double left,
                          double right,
                          double top,
                          double bottom) {
  out << "<polyline fill=\"none\" stroke=\"" << color << "\" stroke-width=\"2\" points=\"";
  for (const ReplayPoint &point : points) {
    double rssi = smooth ? point.smoothRssi : point.rawRssi;
    double x = mapValue(point.seconds, minT, maxT, left, right);
    double y = mapValue(signalLevel(rssi), 0, 99, bottom, top);
    out << std::fixed << std::setprecision(1) << x << "," << y << " ";
  }
  out << "\"/>\n";
}

static void writeSvg(const std::string &path,
                     const std::string &inputPath,
                     const std::vector<ReplayPoint> &points,
                     const PeakDetectorConfig &config,
                     bool detected,
                     double peakSeconds,
                     float peakRssi) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Cannot write graph: " + path);
  }

  const double width = 1100;
  const double height = 520;
  const double left = 58;
  const double right = width - 36;
  const double top = 54;
  const double bottom = height - 54;
  double minT = points.empty() ? 0 : points.front().seconds;
  double maxT = points.empty() ? 1 : points.back().seconds;
  if (maxT <= minT) {
    maxT = minT + 1;
  }

  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
      << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
  out << "<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>\n";
  out << "<text x=\"20\" y=\"24\" font-family=\"system-ui, sans-serif\" font-size=\"16\" fill=\"#20211f\">"
      << xmlEscape(inputPath) << "</text>\n";
  out << "<text x=\"20\" y=\"44\" font-family=\"system-ui, sans-serif\" font-size=\"13\" fill=\"#555b52\">"
      << "SMA " << (int)config.movingAverageMeasurements
      << ", EMA " << (int)config.emaAlphaPercent << "%"
      << ", rise " << (int)config.peakArmRiseDb
      << " dB, drop " << (int)config.peakDropDb
      << " dB, drop readings " << (int)config.requiredDropReadings
      << "</text>\n";

  for (int level = 10; level <= 90; level += 10) {
    double y = mapValue(level, 0, 99, bottom, top);
    out << "<line x1=\"" << left << "\" y1=\"" << y << "\" x2=\"" << right
        << "\" y2=\"" << y << "\" stroke=\"#e1e4df\"/>\n";
    out << "<text x=\"22\" y=\"" << y + 4
        << "\" font-family=\"system-ui, sans-serif\" font-size=\"12\" fill=\"#495047\">"
        << level << "</text>\n";
  }
  out << "<text x=\"20\" y=\"" << height - 18
      << "\" font-family=\"system-ui, sans-serif\" font-size=\"12\" fill=\"#495047\">signal 0-99</text>\n";

  if (!points.empty()) {
    writePolyline(out, points, false, "#1f6feb", minT, maxT, left, right, top, bottom);
    writePolyline(out, points, true, "#d1242f", minT, maxT, left, right, top, bottom);

    if (detected) {
      double x = mapValue(peakSeconds, minT, maxT, left, right);
      out << "<line x1=\"" << x << "\" y1=\"" << top << "\" x2=\"" << x
          << "\" y2=\"" << bottom << "\" stroke=\"#7a5c00\" stroke-width=\"2\"/>\n";
      out << "<text x=\"" << std::min(x + 8, right - 160.0) << "\" y=\"72\""
          << " font-family=\"system-ui, sans-serif\" font-size=\"15\" fill=\"#7a5c00\">Peak "
          << std::fixed << std::setprecision(3) << peakSeconds << " s, "
          << std::setprecision(1) << peakRssi << " dBm</text>\n";
    }
  }

  out << "<text x=\"" << right - 260 << "\" y=\"" << height - 18
      << "\" font-family=\"system-ui, sans-serif\" font-size=\"12\" fill=\"#1f6feb\">raw</text>\n";
  out << "<text x=\"" << right - 220 << "\" y=\"" << height - 18
      << "\" font-family=\"system-ui, sans-serif\" font-size=\"12\" fill=\"#d1242f\">smoothed</text>\n";
  out << "</svg>\n";
}

static std::string argValue(int argc, char **argv, const std::string &name, const std::string &fallback) {
  for (int i = 1; i + 1 < argc; i++) {
    if (argv[i] == name) {
      return argv[i + 1];
    }
  }
  return fallback;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: replay_peak input.csv [output.svg] [--avg N] [--ema N] [--rise N] [--drop N] [--drop-readings N]\n";
    return 2;
  }

  std::string inputPath = argv[1];
  std::string outputPath = argc >= 3 && argv[2][0] != '-' ? argv[2] : "peak_replay.svg";
  PeakDetectorConfig config = {
    (uint8_t)std::stoi(argValue(argc, argv, "--avg", "3")),
    (uint8_t)std::stoi(argValue(argc, argv, "--ema", "50")),
    (uint8_t)std::stoi(argValue(argc, argv, "--rise", "6")),
    (uint8_t)std::stoi(argValue(argc, argv, "--drop", "5")),
    (uint8_t)std::stoi(argValue(argc, argv, "--drop-readings", "3")),
    false
  };

  std::ifstream input(inputPath);
  if (!input) {
    std::cerr << "Cannot open " << inputPath << "\n";
    return 1;
  }

  std::string line;
  if (!std::getline(input, line)) {
    std::cerr << "Empty CSV\n";
    return 1;
  }

  std::vector<std::string> header = splitCsvLine(line);
  int timeColumn = columnIndex(header, "time_ms");
  int secondsColumn = columnIndex(header, "seconds");
  int rawColumn = columnIndex(header, "raw_rssi");
  if (rawColumn < 0) {
    rawColumn = columnIndex(header, "rssi");
  }
  if (rawColumn < 0 || (timeColumn < 0 && secondsColumn < 0)) {
    std::cerr << "CSV must include raw_rssi or rssi, and time_ms or seconds columns\n";
    return 1;
  }

  PeakDetector detector;
  detector.configure(config);
  detector.reset(0);

  std::vector<ReplayPoint> points;
  bool detected = false;
  double peakSeconds = 0.0;
  float peakRssi = -100.0f;

  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    std::vector<std::string> cells = splitCsvLine(line);
    if ((int)cells.size() <= rawColumn) {
      continue;
    }

    uint32_t timeMs = 0;
    double seconds = 0.0;
    if (timeColumn >= 0 && (int)cells.size() > timeColumn) {
      timeMs = (uint32_t)std::stoul(cells[timeColumn]);
      seconds = timeMs / 1000.0;
    }
    if (secondsColumn >= 0 && (int)cells.size() > secondsColumn) {
      seconds = std::stod(cells[secondsColumn]);
      if (timeColumn < 0) {
        timeMs = (uint32_t)std::llround(seconds * 1000.0);
      }
    }

    int rawRssi = std::stoi(cells[rawColumn]);
    PeakDetectorResult result = detector.process(timeMs, rawRssi);
    points.push_back({timeMs, seconds, rawRssi, result.smoothRssi, result.peakDetected});

    if (result.peakDetected && !detected) {
      detected = true;
      peakSeconds = result.currentPeakMs / 1000.0;
      if (!points.empty()) {
        double firstSeconds = points.front().seconds;
        peakSeconds = (result.currentPeakMs / 1000.0) - (points.front().timeMs / 1000.0) + firstSeconds;
      }
      peakRssi = result.currentPeakRssi;
    }
  }

  writeSvg(outputPath, inputPath, points, config, detected, peakSeconds, peakRssi);

  std::cout << "input=" << inputPath << "\n";
  std::cout << "samples=" << points.size() << "\n";
  if (detected) {
    std::cout << std::fixed << std::setprecision(3)
              << "peak_time_seconds=" << peakSeconds << "\n"
              << std::setprecision(2)
              << "peak_rssi=" << peakRssi << "\n";
  } else {
    std::cout << "peak_time_seconds=none\n";
  }
  std::cout << "graph=" << outputPath << "\n";

  return 0;
}
