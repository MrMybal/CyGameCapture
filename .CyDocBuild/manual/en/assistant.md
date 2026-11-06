---
id: assistant
title: Ask the AI assistant
description: Ask an installed AI client to propose a buffer and capture moment.
section: User guide
parent: inspector
order: 9
revision: cacf74a5f587
---

The assistant uses an installed, signed-in **Claude Code, Codex or OpenCode** client. It is optional; manual capture works without it. CyGameCapture does not include a model or store a provider API key.

1. On D3D11, show real gameplay with the interface visible.
2. Choose a client in **AI Assistant** and click the search button. A discussion window opens.
3. The add-on samples buffers and moments into a shared GPU image. `CyGameCaptureAI.exe` reads the images outside the game and asks the selected client to identify a complete scene without HUD.
4. Read the explanation and proposal. **Show** selects it for inspection; **Apply** uses it for capture; **Apply, and send the interface alone too** also requests a HUD-derived stream.
5. Reply with details such as “subtitles remain”. Keep **Attach the current frame** on to send fresh images. Send with the button or `Ctrl`+`Enter`. **New discussion** clears the conversation.

The proposal includes a buffer ID and, where appropriate, a write moment. It is checked against the sampled frame. Inspect it: a scene change can invalidate the recommendation. Black loading screens, obscured gameplay or absent HUD give the model too little evidence.

## Privacy and diagnosis {#privacy-and-diagnosis}

Your chosen client’s provider receives the sampled pictures, buffer metadata and conversation through your account. The last 12 messages can accompany the next request. Client activity may use your provider allowance. Review local files at `%LOCALAPPDATA%\CyGameCapture\AI\<game>`: `prompt.txt`, `answer.txt` and `picture_NN.png`. They can contain game images and conversation text. Delete them when no longer needed; redact them before sharing.

Optional settings: `AIClient`, `AIModel`, `AIClientPath` and `AILanguage`. If a client is missing, install/sign in to it separately, then select its executable path if automatic discovery fails. If the helper is absent, restore it next to the add-on. Analysis at frame moments is not available in D3D12.
