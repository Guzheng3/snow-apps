using System.Text.Json;
using WeChatOcr;

if (args.Length < 1)
{
    Console.Error.WriteLine("Usage: snowshot-wechat-ocr <image-path> [--wechat-dir <path>]");
    Environment.Exit(1);
}

var imagePath = args[0];
string? wechatDir = null;

for (int i = 1; i < args.Length - 1; i++)
{
    if (args[i] == "--wechat-dir" && i + 1 < args.Length)
        wechatDir = args[i + 1];
}

if (!File.Exists(imagePath))
{
    Console.Error.WriteLine($"Image not found: {imagePath}");
    Environment.Exit(2);
}

try
{
    var tcs = new TaskCompletionSource<string>();
    var results = new List<object>();

    using var ocr = wechatDir != null
        ? new ImageOcr(wechatDir)
        : new ImageOcr();

    ocr.Run(imagePath, (path, result) =>
    {
        if (result?.OcrResult?.SingleResult == null)
        {
            tcs.TrySetResult("[]");
            return;
        }

        var blocks = new List<object>();
        foreach (var item in result.OcrResult.SingleResult)
        {
            if (string.IsNullOrEmpty(item.SingleStrUtf8))
                continue;

            blocks.Add(new
            {
                box_points = new[]
                {
                    new { x = item.Left, y = item.Top },
                    new { x = item.Right, y = item.Top },
                    new { x = item.Right, y = item.Bottom },
                    new { x = item.Left, y = item.Bottom },
                },
                text = item.SingleStrUtf8,
                text_score = 1.0f,
            });
        }

        var output = new
        {
            text_blocks = blocks,
            scale_factor = 1.0f,
        };

        tcs.TrySetResult(JsonSerializer.Serialize(output));
    });

    // Wait with timeout (30 seconds)
    var completed = await Task.WhenAny(tcs.Task, Task.Delay(30_000));
    if (completed == tcs.Task)
    {
        Console.WriteLine(await tcs.Task);
    }
    else
    {
        Console.Error.WriteLine("OCR timed out after 30 seconds");
        Environment.Exit(3);
    }
}
catch (Exception ex)
{
    Console.Error.WriteLine($"OCR error: {ex.Message}");
    Environment.Exit(4);
}