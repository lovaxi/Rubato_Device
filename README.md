# Thinktime

English | [简体中文](README.zh-CN.md)

Thinktime makes your AI's working state visible — on a physical desk device.
![Thinktime device](assets/product.jpg)



```
model call ──> Thinktime plugin ──> EMQX broker ──> Thinktime device (TFT display)
```
## MQTT contract

All plugins publish the same messages on `thinktime/<deviceId>/state` —
one topic per device. Auth mirrors the device firmware: username = deviceId
(`TT-xxxxxx`), password = its token. MQTT 3.1.1 over TLS (8883), one
persistent connection.

| Message | Payload | Notes |
|---|---|---|
| `Estimate` | `{ model, state, ts, estSec }` | predicted duration in seconds |
| `Thinking` | `{ model, state, ts }` | first reasoning chunk |
| `Generating` | `{ model, state, ts }` | first answer/tool chunk |
| `Done` | `{ model, state, ts }` | intermediate tool-call steps do not emit Done |

The device shows the model name and a breathing orb that changes color per
phase — slow cream while thinking, faster ice-blue while generating, a green
flash when done. When the estimate predicts a long run during working hours,
the device shows a full-screen health reminder (time to drink water / rest).

## License

[GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.html)
