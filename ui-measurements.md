# Transistor Distortion UI Measurements

Reference document for creating custom background graphics.

## Window Size

| Property | Value |
|----------|-------|
| Width    | 590 px |
| Height   | 300 px |

## Header Area

| Element | Position | Notes |
|---------|----------|-------|
| Header strip | y: 0–58 px | Dark background strip |
| Title "TRANSISTOR DISTORTION" | x: 24, y: 24 | Left-aligned |
| Subtitle | x: 24, y: 42 | Left-aligned |

## Slider Dimensions

All sliders share the same dimensions:

| Property | Value |
|----------|-------|
| Width    | 88 px |
| Height   | 108 px |

## Top Row (5 sliders)

**Row Y Position:** 72 px

| Slider   | X Position | Bounds (x, y, w, h) |
|----------|------------|---------------------|
| GATE     | 26         | 26, 72, 88, 108     |
| INPUT    | 126        | 126, 72, 88, 108    |
| DRIVE    | 226        | 226, 72, 88, 108    |
| TONE     | 326        | 326, 72, 88, 108    |
| ENV MOD  | 426        | 426, 72, 88, 108    |

## Bottom Row (4 sliders, centered)

**Row Y Position:** 184 px

| Slider   | X Position | Bounds (x, y, w, h) |
|----------|------------|---------------------|
| RESO     | 82         | 82, 184, 88, 108    |
| LOW CUT  | 194        | 194, 184, 88, 108   |
| OUTPUT   | 306        | 306, 184, 88, 108   |
| MIX      | 418        | 418, 184, 88, 108   |

## Slider Internal Layout

Positions relative to each slider's top-left corner (x, y):

| Element      | Position                          | Size          |
|--------------|-----------------------------------|---------------|
| Label badge  | x + 6, y + 6                      | 76 x 18 px    |
| Track top    | x + 44 (center), y + 30           | —             |
| Track bottom | x + 44 (center), y + 80           | —             |
| Track height | —                                 | 50 px         |
| Value box    | x + 6, y + 84                     | 76 x 18 px    |

## Colors

| Element              | RGB              | Hex       |
|----------------------|------------------|-----------|
| Accent (orange)      | 235, 140, 36     | `#EB8C24` |
| Background top       | 20, 22, 18       | `#141612` |
| Background bottom    | 35, 30, 24       | `#231E18` |
| Panel (slider bg)    | 36, 38, 33       | `#242621` |
| Text                 | 235, 230, 219    | `#EBE6DB` |
| Dim text             | 158, 158, 148    | `#9E9E94` |
| Header strip         | 20, 23, 20       | `#141714` |

## Visual Layout Diagram

```
┌──────────────────────────────────────────────────────────────┐
│  TRANSISTOR DISTORTION                                  0-58 │
│  Gate-driven grit with resonant tone shaping                 │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐           │
│  │ GATE │  │INPUT │  │DRIVE │  │ TONE │  │ENVMOD│    72-180 │
│  │      │  │      │  │      │  │      │  │      │           │
│  └──────┘  └──────┘  └──────┘  └──────┘  └──────┘           │
│     26       126       226       326       426               │
│                                                              │
│      ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐                 │
│      │ RESO │  │LOWCUT│  │OUTPUT│  │ MIX  │          184-292│
│      │      │  │      │  │      │  │      │                 │
│      └──────┘  └──────┘  └──────┘  └──────┘                 │
│         82       194       306       418                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
                           590 x 300
```
