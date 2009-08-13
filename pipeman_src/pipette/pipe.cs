
using System;
using System.Collections.Generic;
namespace pipette
{
	class copyStart
	{
		public PipeEndType fromPipeEndType;
		public PipeEndType toPipeEndType;
		public deepID fromDeepId;
		public deepID toDeepId;
	}

	public class Pipe
	{
		public delegate void superAbortHandler();
		public event superAbortHandler superAbort;
		//some factory methods
		public static Pipe service_pipe(out int bind_port,string rHostname, int rPort, bool iChopYourDollar, string identity, string svcname, string m0therkey)
		{
			bind_port = magic.random_port();
			PipeEndType fromEnd = new PipeEndTypeLocalBind(System.Net.IPAddress.Any,bind_port,new AuthenticationTypeServiceAccept(iChopYourDollar,identity,svcname,m0therkey));
			PipeEndType toEnd = new PipeEndTypeTCPClient(rHostname,rPort,new AuthenticationTypeNone(false));
			Pipe p = new Pipe(fromEnd,toEnd);
			return p;
		}
		public static Pipe connect_pipe(out int bind_port,string hostname, int port, bool iChopYourDollar,string otpkey, string otp)
		{
			bind_port = magic.random_port();
			PipeEndType fromEnd = new PipeEndTypeLocalBind(System.Net.IPAddress.Loopback,bind_port,new AuthenticationTypeNone(false));
			PipeEndType toEnd = new PipeEndTypeTCPClient(hostname,port,new AuthenticationTypeServiceConnect(iChopYourDollar,otpkey,otp));
			Pipe p = new Pipe(fromEnd,toEnd);
			return p;
		}
		static int buffer_size = 8192;
		PipeEndType from;
		PipeEndType to;
		bool pipeOpen = false;
		List<System.Threading.Thread> runningThreads = new List<System.Threading.Thread>();
		//Dictionary<deepID,deepID> tofrom = new Dictionary<deepID, deepID>();
		//Dictionary<PipeEndType,PipeEndType> fromto = new Dictionary<PipeEndType, PipeEndType>();

		/// <summary>
		/// 
		/// </summary>
		/// <param name="from">
		/// A <see cref="PipeEndType"/>
		/// This connection is attempted as soon as start() is called
		/// </param>
		/// <param name="to">
		/// This connection is attempted when from decides it is necessary
		/// A <see cref="PipeEndType"/>
		/// </param>
		public Pipe(PipeEndType from, PipeEndType to)
		{
			
			this.from = from;
			this.to = to;
		}
		void establishedTo(deepID toID, deepID fromID)
		{
			
				logger.logger.info("Established 'to' pipe-end: " + to.description(toID));
			try
			{
				to.performAuthentication(toID);
			}
			catch(VerificationFailedException e)
			{
				logger.logger.warn("Verification on 'to' end failed.  Goodbye.");
				to.closeConnection(toID);
				from.closeConnection(fromID);
			}
				logger.logger.info("Pipe seems to be up.");
				
				
				//from->to
				copyStart fromTo = new copyStart();
				fromTo.fromDeepId = fromID;
				fromTo.fromPipeEndType = from;
				fromTo.toDeepId = toID;
				fromTo.toPipeEndType = to;
				System.Threading.Thread ft = new System.Threading.Thread(copyThread);
				ft.Start(fromTo);
				fromID.read_thread = ft;
				//to->from
				copyStart toFrom = new copyStart();
				toFrom.fromDeepId = toID;
				toFrom.fromPipeEndType = to;
				toFrom.toDeepId = fromID;
				toFrom.toPipeEndType = from;
				System.Threading.Thread tf = new System.Threading.Thread(copyThread);
				tf.Start(toFrom);
				toID.read_thread = tf;
			
				from.addconnectionClosedHandler(delegate(deepID d) {
					logger.logger.info("From connection was forcibly closed.");

					toID.read_thread.Abort();
					to.closeConnection(toID);
					if (superAbort != null) superAbort();
					d.read_thread.Abort();
					ft.Abort(); //this is our thread... yeah it's convoluted

				});
			logger.logger.debug("Registering to handler...");
				to.addconnectionClosedHandler(delegate(deepID d) {
				
					logger.logger.info("To connection was forcibly closed.");
				logger.logger.debug(new System.Diagnostics.StackTrace().ToString());
				//	fromID.read_thread.Abort();
				if (superAbort != null) superAbort();
					d.read_thread.Abort(); //this is us... otherewise we'll keep reading a closed socket
				});
		}
		void establishedFrom(deepID fromID, deepID related)
		{
			logger.logger.info("Established 'from' pipe-end: " + from.description(fromID));

			if (related != null)
			{
				throw new Exception("From connection has a relationship!");
			}
			try
			{
				from.performAuthentication(fromID);
			}
			catch (VerificationFailedException e)
			{
				logger.logger.warn("Verification on 'from' end failed.  Goodbye.");
				from.closeConnection(fromID);
				return;
			}
			to.start(fromID);
		}
		public void copyThread(object o)
		{
			copyStart s = (copyStart) o;
			while(true)
			{
				s.fromPipeEndType.copyTo(s.fromDeepId,s.toDeepId,s.toPipeEndType);
			}
		}
		public void start()
		{
			logger.logger.info("Starting up pipe...");
			from.endpointEstablished += establishedFrom;
			to.endpointEstablished += establishedTo;
			from.start(null);
			//throw new Exception("startCrash");
		}
		public void stop()
		{
			logger.logger.info("Bringing down threads[" + runningThreads.Count + "]");
		/*	foreach(System.Threading.Thread t in runningThreads)
			{
				if (t==System.Threading.Thread.CurrentThread)
					throw new Exception("Can't abort myself!");
				t.Abort();
				logger.logger.debug("thread down");
			}*/
			/*to.stop();
			logger.logger.debug("to down");*/
			//from.stop();
			logger.logger.debug("from down");
			logger.logger.info("stopped");
		}


		
		
		
	}
}
