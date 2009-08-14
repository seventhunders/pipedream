
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
		//List<deepID> suspended_from_by_me = new List<deepID>();
		List<deepID> suspended_to_by_me = new List<deepID>();
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
				
				authenticatedTo (fromID,toID);
		}

		void authenticatedTo (pipette.deepID fromID, pipette.deepID toID)
		
		{
			copyStart fromTo = new copyStart ();
			fromTo.fromDeepId = fromID;
			fromTo.fromPipeEndType = from;
			fromTo.toDeepId = toID;
			fromTo.toPipeEndType = to;
			System.Threading.Thread ft = new System.Threading.Thread (copyThread);
			ft.Start (fromTo);
			fromID.read_thread = ft;
			copyStart toFrom = new copyStart ();
			toFrom.fromDeepId = toID;
			toFrom.fromPipeEndType = to;
			toFrom.toDeepId = fromID;
			toFrom.toPipeEndType = from;
			System.Threading.Thread tf = new System.Threading.Thread (copyThread);
			tf.Start (toFrom);
			toID.read_thread = tf;
			//ok, now for cakeisalie protocol, for those that support it
			fromID.unregisterallCakeIsALie_remoteEndHungupHandlers(); //incase this fromID is being recycled...
			fromID.cakeisalie_remoteEndHungup += delegate(deepID who) {
				//hang up to connection. todo: make this support cakeisalie on both sides
				toID.read_thread.Abort();
				to.closeConnection(toID);
				//wait for a resume
				(from.authenticationType as AuthenticationTypeAESLike).spinForResumeMsg(fromID);
				authenticatedFrom(fromID);
				//authenticatedFrom is going to bring up a whole host of threads, including a new copythread (which we're in at this point).
				System.Threading.Thread.CurrentThread.Abort(); 
			};
			toID.unregisterallCakeIsALie_remoteEndHungupHandlers(); //in case this toID is being recycled...
			toID.cakeisalie_remoteEndHungup += delegate(deepID who) {
				fromID.read_thread.Abort();
				from.closeConnection(fromID);
				//the next time we get a 'from' connection, we'll recycle this 'to'
				suspended_to_by_me.Add(toID);
				System.Threading.Thread.CurrentThread.Abort();
			};
			
			
			fromID.unregisterAllConnectionClosedHandlers(); //incase this fromID is being recycled...
			fromID.connectionClosed += (delegate(deepID d) {
				logger.logger.info ("From connection was forcibly closed.");
				if (superAbort != null)
					superAbort (); 
				else if (to.authenticationType is AuthenticationTypeAESLike) { //knows cakeisalie protocol
					
					//inform gateway there was a disconnection
					(to.authenticationType as AuthenticationTypeAESLike).sendCakeIsALieMsg (AuthenticationTypeAESLike.remote_end_hungup, toID);
					//when we get a new establishedFrom, we need to re-use this existing 'to' pipeEnd.
					suspended_to_by_me.Add (toID);
					System.Threading.Thread.CurrentThread.Abort();
				} else {
					toID.read_thread.Abort ();
					to.closeConnection (toID);
				}
				d.read_thread.Abort ();
				ft.Abort ();
			});
			logger.logger.debug ("Registering to handler...");
			toID.unregisterAllConnectionClosedHandlers(); //incase this toID is being recycled...
			toID.connectionClosed += delegate(deepID d) {
				logger.logger.info ("To connection was forcibly closed.");
				logger.logger.debug (new System.Diagnostics.StackTrace ().ToString ());
				if (superAbort != null)
					superAbort (); 
				else if (@from.authenticationType is AuthenticationTypeAESLike)//knows cakeisalie protocol
				{
					//inform client that there was a remote disconnection
					(@from.authenticationType as AuthenticationTypeAESLike).sendCakeIsALieMsg (AuthenticationTypeAESLike.remote_end_hungup, fromID);
					//wait for client to attempt to reconnect
					(from.authenticationType as AuthenticationTypeAESLike).spinForResumeMsg(fromID);
					//resume from a post-authenticated world
					authenticatedFrom(fromID);
					System.Threading.Thread.CurrentThread.Abort();
				} else {
					fromID.read_thread.Abort ();
					@from.closeConnection (fromID);
				}
				d.read_thread.Abort ();
			};
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
			authenticatedFrom(fromID);
			
		}
		void authenticatedFrom(deepID fromID)
		{
			
			if (suspended_to_by_me.Count > 0) //we're going to resume the 'to' pipeend from the cakeisalie backlog
			{
				deepID toID = suspended_to_by_me[0];
				suspended_to_by_me.RemoveAt(0);
				(to.authenticationType as AuthenticationTypeAESLike).sendCakeIsALieMsg(AuthenticationTypeAESLike.remote_end_resumed,toID); //inform the gateway that we're back up
				authenticatedTo(fromID,toID); //start from a post-authenticated 'to' pipe
			}
			try
			{
				to.start(fromID);
			}
			catch(Exception ex)
			{
				logger.logger.warn("An error ocurred establishing the 'to' end.  Closing connection...");
				from.closeConnection(fromID);

			}
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
			//logger.logger.info("Bringing down threads[" + runningThreads.Count + "]");
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
