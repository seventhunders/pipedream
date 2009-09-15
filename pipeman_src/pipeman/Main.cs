using System;

namespace pipeman
{
	class MainClass
	{
		public static void Main (string[] args)
		{
			Console.Error.WriteLine("pipeman v" + System.Reflection.Assembly.GetExecutingAssembly().GetName().Version);
			Console.Error.WriteLine("Copyright 2009 DefyCensorship.com.  Pipedream released under the MIT license.");
			
			if (search("gateway",args))
			{
				string RSA = expectRSA(args);
				pipette.gatewayPipe g = new pipette.gatewayPipe(RSA);
				g.start();
				hang();
			}
			else if (search("m0ther",args))
			{
				string RSA = expectRSA(args).Trim();
				string identity = expectIdentity(args).Trim();
				int port;
				pipette.m0therpipe m = pipette.m0therpipe.bind_m0ther(out port,identity,RSA);
				logger.logger.machineReadable(port+"","m0therbound");
				hang();
			}
			else if (search("acceptsvc",args))
			{
				int bind_port;
				string rHostname = expect("remote-hostname",args);
				int rPort = int.Parse(expect("remote-port",args));
				string identity = expectIdentity(args);
				string svcname = expect("service-name",args);
				string RSA = expectRSA(args);
				pipette.Pipe s = pipette.Pipe.service_pipe(out bind_port, rHostname,rPort,false,identity,svcname,RSA);
				s.start();
				logger.logger.machineReadable(bind_port + "","svcbound");
				hang();
			}
			else if (search("connectsvc",args))
			{
				int bind_port;
				string rHostname = expect("remote-hostname",args);
				int rPort = int.Parse(expect("remote-port",args));
				string identity = expectIdentity(args);
				string otpkey = expect("otp-key",args);
				string otp = expect("otp",args);
				pipette.Pipe c = pipette.Pipe.connect_pipe(out bind_port,rHostname,rPort,false,otpkey,otp);
				c.superAbort += delegate {
					System.Environment.Exit(0);	
				};
				c.start();
				logger.logger.machineReadable(bind_port + "","connectbound");
				hang();
			}
			else if (search("makersa",args))
			{
				cryptlib.AsymmetricKey a = cryptlib.RSA.randomRSAKey();
				Console.WriteLine(a.publicKey);
				Console.WriteLine("-----PRIVATE-------KEY-------BELOW");
				Console.WriteLine(a.privateKey);
			}
			else
			{
				throw new Exception("I don't understand what do you mean");
			}
			Console.WriteLine("pipeman out.");
		}
		static string expectRSA(string[] args)
		{
			return strip("--rsa=",args,true);
		}
		static string expectIdentity(string[] args)
		{
			return strip("--identity=",args,true);
		}
		static string expect(string flag,string[] args)
		{
			return strip("--" + flag + "=",args,true);
		}
		static string getarg(string prefix, string[] args, string defaultv)
		{
			string r = strip(prefix,args,false);
			if (r==null) return defaultv;
			return r;
		}
		static string strip(string prefix, string[] args, bool except)
		{
			foreach(string s in args)
			{
				if (s.StartsWith(prefix))
				{
					return s.Substring(prefix.Length);
				}
			}
			if (except)
				throw new Exception("Expected argument: " + prefix);
			return null;
		}
		static bool search(string prefix, string[] args)
		{
			foreach (string s in args)
			{
				 if (s.StartsWith(prefix)) return true;
			}
			return false;
		}
		static void hang()
		{
			while(true)
				{
					System.Threading.Thread.Sleep(60*1000);
				}
		}
	}
}
