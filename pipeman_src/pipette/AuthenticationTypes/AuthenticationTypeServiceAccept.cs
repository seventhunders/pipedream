
using System;

namespace pipette
{

/// <summary>
/// This is the authentication type used by services accepting clients
/// </summary>
	public class AuthenticationTypeServiceAccept: AuthenticationTypeAESLike
	{
		internal static byte[] salt = System.Text.Encoding.ASCII.GetBytes("Seal up what the seven thunders said, and no not write it down!");
		string identity;
		string svcname;
		string m0therkey;
		public AuthenticationTypeServiceAccept (bool iChopYourDollar, string identity, string svcname, string m0therkey): base(iChopYourDollar)
		{
			this.identity = identity;
			this.svcname = svcname;
			this.m0therkey = m0therkey;
		}
		public override bool shouldAllowPerson (System.Net.IPEndPoint p)
		{
			logger.logger.push("shouldAllowPersonAccept");
			logger.logger.debug("Attempting to verify your ip info with m0ther...");
			if (!base.shouldAllowPerson(p))
				return false;

			int mport;
			Pipe m = m0therpipe.bind_m0ther(out mport,identity,m0therkey);
			string connecting_uri = System.Web.HttpUtility.UrlEncode("tcp://" + p.Address.ToString());
			string safe_id = System.Web.HttpUtility.UrlEncode(this.identity);
			string safe_service = System.Web.HttpUtility.UrlEncode(svcname);
			string apiurl = "/api/connect?connecting_uri=" + connecting_uri + "&identity=" + safe_id + "&service=" + safe_service;
		//	System.Net.ServicePointManager.ServerCertificateValidationCallback = new System.Net.Security.RemoteCertificateValidationCallback(BypassSslCertificateValidation);
			//System.Net.WebClient c = new System.Net.WebClient();
			//string result = c.DownloadString("http://localhost:" + mport + apiurl);
			string result = m0ther_api_request.req(mport,apiurl);
			m.stop();
			logger.logger.debug("really stopped.");
			if (result != "OK")
			{
				logger.logger.debug("Computer says no");
				logger.logger.pop();
				return false;
			}
			logger.logger.debug("Veririfed your ip info with m0ther");
			logger.logger.pop();
			return true;
		}
		public override void shouldVerifyIdentity (deepID d)
		{
			logger.logger.push("shouldVerifyIdentityAccept");
			byte[] bkey = msgBase.read(d.plaintextStream);
			string otpkey = System.Text.Encoding.ASCII.GetString(bkey,0,bkey.Length);
			//need the otp
			int mport;
			logger.logger.debug("right0");
			Pipe m = m0therpipe.bind_m0ther(out mport, identity, m0therkey);
			logger.logger.debug("motherbound");
			string safe_identity = System.Web.HttpUtility.UrlEncode(identity);
			string safe_service = System.Web.HttpUtility.UrlEncode(svcname);
			string safe_otp = System.Web.HttpUtility.UrlEncode(otpkey);
			string apiurl = "/api/otp?identity="+safe_identity + "&service="+safe_service+"&otp="+safe_otp;
			
			//System.Net.ServicePointManager.ServerCertificateValidationCallback = new System.Net.Security.RemoteCertificateValidationCallback(BypassSslCertificateValidation);
			string otk = m0ther_api_request.req(mport,apiurl);
			m.stop();
			logger.logger.debug("I'm using otk " + otk);
			System.Security.Cryptography.Rfc2898DeriveBytes derive = new System.Security.Cryptography.Rfc2898DeriveBytes(otk,salt);
			d.aeskey = derive.GetBytes(cryptlib.AES.total_key_size);
			logger.logger.pop();
			
		}
	internal static bool BypassSslCertificateValidation(object sender, System.Security.Cryptography.X509Certificates.X509Certificate cert, 
		                                                    System.Security.Cryptography.X509Certificates.X509Chain chain, System.Net.Security.SslPolicyErrors error) {
  		return true;
	}

		

	}
}
