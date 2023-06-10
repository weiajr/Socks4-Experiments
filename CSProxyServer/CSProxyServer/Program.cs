using System.Net;
using System.Net.Sockets;

var socket = new TcpListener(IPAddress.Any, 1080);
socket.Start();

while (true)
{
    var client = await socket.AcceptTcpClientAsync();
    Console.WriteLine($"[{DateTime.Now.ToString("T")}] handeling for {((IPEndPoint)client.Client.RemoteEndPoint).Address}");
    Task.Run(() => HandleClient(client));
}

async Task HandleClient(TcpClient client)
{
    using var tokenSrc = new CancellationTokenSource();

    using var fromStream = client.GetStream();

    byte[] buffer = new byte[1024];
    int bytesRead = await fromStream.ReadAsync(buffer, 0, buffer.Length, tokenSrc.Token);

    using var ms = new MemoryStream(buffer);
    using var bin = new BinaryReader(ms);
    bin.BaseStream.Seek(1, SeekOrigin.Begin);

    if (bin.ReadByte() != 0x1)
    {
        await fromStream.WriteAsync(new byte[] { 0, 0x5b, 0, 0, 0, 0, 0, 0 }, tokenSrc.Token);
        client.Close();
        return;
    }

    int port = IPAddress.NetworkToHostOrder(bin.ReadInt16());
    var dst = new IPAddress(bin.ReadUInt32());

    var destinationClient = new TcpClient();
    await destinationClient.ConnectAsync(dst, port, tokenSrc.Token);
    await fromStream.WriteAsync(new byte[] { 0, 0x5a, 0, 0, 0, 0, 0, 0 }, tokenSrc.Token);
    using var destinationStream = destinationClient.GetStream();

    await Task.WhenAny(StreamForwarder(fromStream, destinationStream, tokenSrc.Token),
        StreamForwarder(destinationStream, fromStream, tokenSrc.Token));

    client.Close();
    tokenSrc.Cancel();
}

async Task StreamForwarder(NetworkStream from, NetworkStream dest, CancellationToken token)
{
    int read = 0;
    byte[] buff = new byte[1024];
    while ((read = await from.ReadAsync(buff, 0, buff.Length, token)) != 0 && !token.IsCancellationRequested)
        await dest.WriteAsync(buff, 0, read, token);
}