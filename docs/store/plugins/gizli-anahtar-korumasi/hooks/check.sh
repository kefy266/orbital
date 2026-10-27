#!/bin/bash
# beforePrompt hook: stdin = {"text","sessionID","directory"} JSON.
# Exit non-zero to block (stderr is shown); print text to stdout to replace the prompt.
input=$(cat)
if grep -qE '(sk-[A-Za-z0-9_-]{20,}|sk-ant-[A-Za-z0-9_-]{20,}|AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{36}|-----BEGIN [A-Z ]*PRIVATE KEY-----)' <<<"$input"; then
  echo "Mesajda bir API anahtarı veya özel anahtar var gibi görünüyor; güvenlik için gönderilmedi." >&2
  exit 1
fi
exit 0
