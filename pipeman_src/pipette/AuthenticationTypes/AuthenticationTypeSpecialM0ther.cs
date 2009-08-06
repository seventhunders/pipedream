
using System;

namespace pipette
{
		
	/// <summary>
	/// NOT FOR GENERAL USE!
	/// </summary>
	public class AuthenticationTypeSpecialM0ther: AuthenticationTypeAESLike
	{
		string identity;
		string m0therkey;
		public AuthenticationTypeSpecialM0ther (string identity, string m0therkey): base(true)
		{
			this.identity = identity;
			this.m0therkey = m0therkey;
		}
		public override bool shouldAllowPerson (System.Net.IPEndPoint p)
		{
			if (!base.shouldAllowPerson(p)) return false;
			logger.logger.warn("AuthenticationTypeSpecialM0ther allowing external connection...");
			return true;
		}
		public override void shouldVerifyIdentity (deepID d)
		{
			//throw new Exception("You fail at life!");
			byte[] aeskey = cryptlib.AES.newAESKey();
			string b64identity = Convert.ToBase64String(System.Text.Encoding.ASCII.GetBytes(identity));
			string b64key = Convert.ToBase64String(aeskey);
			DateTime dstr = DateTime.Now.ToUniversalTime();
			string noReplayAttack = dstr.ToString();
			string challenge = b64identity + "\n" + b64key + "\n" + noReplayAttack;
			byte[] echallenge = cryptlib.RSA.encrypt(System.Text.Encoding.ASCII.GetBytes(challenge),m0therkey);
			msgBase.write(d.plaintextStream,echallenge);
			d.aeskey = aeskey;
			//throw new System.NotImplementedException ();
		}




	}
}
