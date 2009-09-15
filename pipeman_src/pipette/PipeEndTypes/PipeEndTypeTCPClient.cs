
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
			//c.ReceiveTimeout = 5000;
			if (!authenticationType.shouldAllowPerson(new IPEndPoint(remoteAddress,port)))
			{
				throw new Exception("This person is not allowed.");
			}
			
			System.Threading.Thread t = new System.Threading.Thread(superStart);
			t.Start();
			DateTime started = DateTime.Now;
			while(true)
			{
				if ((DateTime.Now - started).TotalSeconds >=2)
					throw new Exception("Connection timed out.");
				if (!t.IsAlive) break;
			}
			while(!c.Connected)
			{
				logger.logger.debug("It's not immediately clear how this situation could occur.");
			}
			deepID d = new deepID();
			d.internal_use = c;
			try{
			d.plaintextStream = c.GetStream();
			} catch (Exception ex)
			{
				logger.logger.warn("There was an error getting the stream: " + ex.Message);
				logger.logger.warn(ex.StackTrace.ToString());
				start(related);
				return;
			}
			d.readZeroBytes += readZeroBytes;
			this.raiseEndpointEstablishedHandler(d,related);
		}
		private void superStart()
		{
			c.Connect(remoteAddress,port);
		}

		void readZeroBytes (deepID d)
		{
			TcpClient c = (TcpClient) d.internal_use;
			logger.logger.debug("client read zero bytes...");
			try
			{
				if (c.Client.Poll(5000,SelectMode.SelectRead))
				{
					if (c.Client.Available==0)
						d.raiseConnectionClosed();
				}
			}
				catch (Exception e)
				{
					logger.logger.warn("Encountered an error while polling to see if the socket is closed.  Specifically:");
					logger.logger.warn(e.Message);
				if (!(e is System.Threading.ThreadAbortException))
				{
					logger.logger.warn("Assuming that the connection is closed.");
					d.raiseConnectionClosed();
				}
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
		}






	}
}
