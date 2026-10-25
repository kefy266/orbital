# beforePrompt hook: stdin = {"text","sessionID","directory"} JSON.
# Exit non-zero to block (stderr is shown); print text to stdout to replace the prompt.

[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$raw = [Console]::In.ReadToEnd()

$pattern = '(sk-[A-Za-z0-9_-]{20,}|sk-ant-[A-Za-z0-9_-]{20,}|AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{36}|-----BEGIN [A-Z ]*PRIVATE KEY-----)'

if ($raw -match $pattern) {
    [Console]::Error.WriteLine("Mesajda bir API anahtarı veya özel anahtar var gibi görünüyor; güvenlik için gönderilmedi.")
    exit 1
}

exit 0
