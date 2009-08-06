
using System;
using System.Net;
using System.Net.Sockets;

namespace pipette
{

/// <summary>
/// Connect via TCP to a remote server
/// </summary>
	public class PipeEndTypeTCPClient: PipeEndType
	{

		IPAddress remoteAddress;
		int port;
		TcpClient c;
		public PipeEndTypeTCPClient(string hostname, int port, AuthenticationType authenticationType): this(Dns.GetHostAddresses(hostname)[0],port,authenticationType)
		{}
		public PipeEndTypeTCPClient (IPAddress remoteAddress, int port, AuthenticationType authenticationType): base(authenticationType)
		{
			this.remoteAddress = remoteAddress;
			this.port = port;
		}
		public override string description (deepID d)
		{
			return remoteAddress.ToString() + ":" + port;
		}

		public override void start (deepID related)
		{
			c = new TcpClient();
			if (!authenticationType.shouldAllowPerson(new IPEndPoint(remoteAddress,port)))
			{
				throw new Exception("This person is not allowed.");
			}
			c.Connect(remoteAddress,port);
			deepID d = new deepID();
			d.internal_use = c;
			d.plaintextStream = c.GetStream();
			d.readZeroBytes += readZeroBytes;
			this.raiseEndpointEstablishedHandler(d,related);
		}

		void readZeroBytes (deepID d)
		{
			TcpClient c = (TcpClient) d.internal_use;
			logger.logger.debug("client read zero bytes...");
			if (c.Client.Poll(1000,SelectMode.SelectRead))
			{
				if (c.Client.Available==0)
					raiseConnectionClosed(d);
			}
		}
		public override void stop ()
		{
			c.Close();
			c = new TcpClient();
		}
		public override void performAuthentication (deepID d)
		{
			authenticationType.shouldVerifyIdentity(d);
		}
		public override void closeConnection (deepID d)
		{
			logger.logger.debug("Shutting down TCP client");
			TcpClient c = (TcpClient) d.internal_use;
			//c.GetStream().Close();
			c.Client.Close();
			base.closeConnection(d);
		}






	}
}
