


using System;
using System.Net.Sockets;
using System.Net;
namespace pipette
{
	internal class private_pass
		
	{
		public TcpListener l;
		public deepID related;
	}
	/// <summary>
	/// Bind to a local port.  
	/// </summary>
	/// 

	public class PipeEndTypeLocalBind: PipeEndType
	{
		TcpListener l;
		AsyncCallback a;
		IPAddress bindAddr;
		int port;
		public PipeEndTypeLocalBind (IPAddress bindAddr, int port, AuthenticationType t): base(t)
		{
			a = new AsyncCallback(gotClient);
			this.bindAddr = bindAddr;
			this.port = port;
			if (bindAddr != IPAddress.Loopback && authenticationType is AuthenticationTypeNone)
			{
				throw new Exception("You're binding to a non-loopback address, and using no authentication.  This is a security disaster.");
			}
			if (bindAddr != IPAddress.Loopback)
			{
				logger.logger.info("You're binding to a non-loopback address on port " + port + ".  This is potentially a security issue, but in theory it's authenticated.");
			}
			//resume
		}
		public override void start(deepID related)
		{
			logger.logger.info("Binding on " + port);
			l = new TcpListener(bindAddr,port);
			l.Start();
			private_pass p = new private_pass();
			p.l = l;
			p.related = related;
			l.BeginAcceptSocket(a,p);
			
		}
		public override void stop()
		{
			l.Stop();
		}
		public override void performAuthentication (deepID d)
		{
			authenticationType.shouldVerifyIdentity(d);
		}
		public override void closeConnection (deepID d)
		{
			Socket s = (Socket) d.internal_use;
			s.Close();
		base.closeConnection(d);
		}
		




		void gotClient(IAsyncResult ar)
		{
							if (l.Server==null || !l.Server.IsBound)
				{
					logger.logger.debug("Just kidding, this was an abort request");
					return;
				}
							private_pass p = (private_pass) ar.AsyncState;

							l.BeginAcceptSocket(a,p);
						logger.logger.debug("here...");
			//for some reason (maybe it's a mono bug?) things in an AsyncCallback thread die silently instead of excepting like they should.  So...
			try
			{

				logger.logger.info("Got client");
				logger.logger.debug(ar.AsyncState.ToString());
				
				l = p.l; //yes, this is redundant, but you never know...

				Socket c = l.EndAcceptSocket(ar);


				//do a sanity check
				if (!authenticationType.shouldAllowPerson(c.RemoteEndPoint as IPEndPoint))
				{
					c.Close();
					return;
				}
				deepID d = new deepID();
				d.internal_use = c;
				d.plaintextStream = new NetworkStream(c);
				d.readZeroBytes += dReadZeroBytes;
				logger.logger.debug("Raising endpointEstablished...");
				raiseEndpointEstablishedHandler(d,p.related);
			
			}
			catch (Exception ex)
			{
				if (ex.InnerException != null)
					logger.logger.warn(ex.Message + "--->" + ex.InnerException.Message);
				else
					logger.logger.warn(ex.Message);

				logger.logger.warn(ex.StackTrace);
				logger.logger.FAIL("gotClient mono bug workaround exception time!");
			}
			
		}
		public override string description (deepID d)
		{
			Socket s = (Socket) d.internal_use;
			return s.RemoteEndPoint.ToString();
		}

		void dReadZeroBytes (deepID d)
		{
			Socket s = (Socket) d.internal_use;
			if (s.Poll(1000,SelectMode.SelectRead)) //well this is awkward
			{
				if (s.Available==0)
					this.raiseConnectionClosed(d);

			}
		}
	}
}
