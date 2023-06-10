using System.Diagnostics;
using System.Net;

using var hand = new HttpClientHandler { Proxy = new WebProxy("socks4://127.0.0.1:1080") };
using var http = new HttpClient(hand);
using var fs = File.OpenWrite("lol.mkv");
fs.SetLength(0);

using var resp = await http.GetAsync("https://cdn.discordapp.com/attachments/993957397016096850/1116486935448395827/Bad_Apple_but_its_Ipv6_1puBUh-T_tw.mkv", HttpCompletionOption.ResponseHeadersRead);
using var stream = await resp.Content.ReadAsStreamAsync();

long respSize = resp.Content.Headers.ContentLength.GetValueOrDefault();

long streamedTotal = 0;
int len = 0;
byte[] buff = new byte[1024 * 8];
var sw = Stopwatch.StartNew();

while ((len = await stream.ReadAsync(buff, 0, buff.Length)) != 0)
{
    await fs.WriteAsync(buff, 0, len);
    streamedTotal += len;
    Console.SetCursorPosition(0, 0);
    Console.WriteLine($"{((double)streamedTotal / respSize * 100d).ToString("0.00")}%, speed ~{((streamedTotal / sw.Elapsed.TotalSeconds) / 1024 / 1024 * 8).ToString("0.00")}Mbit/s, eta {TimeSpan.FromSeconds((double)(respSize - streamedTotal) / (streamedTotal / sw.Elapsed.TotalSeconds)).ToString("mm':'ss")}, received {len}, speed ~{((streamedTotal / sw.Elapsed.TotalSeconds) / 1024 / 1024).ToString("0.00")}MB/s                                  ");
}

fs.Close();