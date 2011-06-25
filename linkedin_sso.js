jQuery(document).ready(function document_ready_cb() {
	jQuery.getScript("https://platform.linkedin.com/in.js?async=true", function framework_loaded_cb() {
		var drupal_linkedin_sso_return_uri = encodeURIComponent(window.location.href.replace(/^http:\/\//, 'https://'));
		var drupal_linkedin_sso_login_uri = Drupal.settings.linkedin_sso.base_url.replace(/^http:\/\//, 'https://') + '/linkedin_sso/login?return=' + drupal_linkedin_sso_return_uri;
		var drupal_linkedin_sso_loggedout_home_uri = Drupal.settings.linkedin_sso.base_url.replace(/^https:\/\//, 'http://');
		var drupal_linkedin_sso_logout_uri = Drupal.settings.linkedin_sso.base_url.replace(/^http:\/\//, 'https://') + '/user/logout?no_jsapi_logout=1';
		
		IN.Event.on(IN, 'frameworkLoaded', function() {
			IN.Event.on(IN, 'logout', function() {
				setTimeout(function() {
					window.location.href = drupal_linkedin_sso_loggedout_home_uri;
				}, 2000);
			});

			if(!IN.User.isAuthorized()) {
				// user isn't LI authed, so register handler for if that happens to redirect to Drupal side login handler
				IN.Event.onOnce(IN, 'auth', function() {
					window.location.href = drupal_linkedin_sso_login_uri;
				});
				
				// if user is logged into Drupal, but not LI, log them out
				if(Drupal.settings.linkedin_sso.active_session) {
					window.location.href = drupal_linkedin_sso_logout_uri;
				}
			} else {
				// if user is LI authed, but doesn't have a drupal active session, redirect them to do Drupal login
				if(!Drupal.settings.linkedin_sso.active_session) {
					window.location.href = drupal_linkedin_sso_login_uri + '&existing_auth=1';
				}
			}
			
			try {
				if(drupal_linkedin_sso_load_callback && drupal_linkedin_sso_load_callback.call) {
					drupal_linkedin_sso_load_callback();
				}
			} catch(e) {
				
			}
		});
				
		// attach LinkedIn SSO login handler
		jQuery('a[href="/user"]', 'a[href=^"/user/login"]').click(function user_login_link_click_handler(event) {
			try { 
				if(!IN.User.authorize()) event.preventDefault();
			}
			catch(e) { 
				return; 
			}
		});

		// registration links point to linkedin.com
		jQuery('a[href=^"/user/register"]').attr({
			href: "http://www.linkedin.com",
			target: "_blank"
		});
		
		IN.init({
		    api_key: Drupal.settings.linkedin_sso.api_key,
			credentials_cookie: "true",
			authorize: "true",
			entropy: Drupal.settings.linkedin_sso.entropy
		});
	});
});



