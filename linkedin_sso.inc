<?php

/**
 * @file
 * This file implements all the functionality from the hooks defined in linkedin_sso.module
 */

class LinkedInSSO {
	private static $instance = null;
	
	private $api_key = null;
	private $api_secret = null;
	private $access_token_url = "https://api.linkedin.com/uas/oauth/accessToken";
	private $profile_api_url = "http://api.linkedin.com/v1/people/~:(id,first-name,last-name,email-address,public-profile-url,picture-url,summary,positions:(title,company:(name)))";
	private $linkedin_sso_entropy = 0;
	
	public static function get_instance() {
		if(!self::$instance) {
			self::$instance = new LinkedInSSO();
		}
		return self::$instance;
	}
	
	private function __construct() {
		$this->api_key = variable_get('linkedin_sso_api_key', null);
		$this->api_secret = variable_get('linkedin_sso_secret_key', null);
		$this->linkedin_sso_entropy = isset($_COOKIE["Drupal_visitor_linkedin_sso_entropy"]) ? $_COOKIE["Drupal_visitor_linkedin_sso_entropy"] : 0;
	}
	
	public function init() {
		drupal_add_js(array('linkedin_sso' => array(
			'api_key' => $this->api_key,
			'base_url' => $GLOBALS["base_url"],
			'active_session' => $GLOBALS["user"] && $GLOBALS["user"]->uid > 0,
			'entropy' => $this->linkedin_sso_entropy,
			'base_path' => base_path()
		)), 'setting');
		
		drupal_add_js(drupal_get_path('module', 'linkedin_sso') . '/linkedin_sso.js', 'file');
	}
	
	public function menu() {
		$items['admin/config/linkedin_sso'] = array(
			'title' => 'LinkedIn SSO',
			'description' => 'LinkedIn SSO settings',
			'page arguments' => array('_linkedin_sso_admin_settings'),
			'page callback' => 'drupal_get_form',
			'access arguments' => array('administer linkedin_sso'),
			'weight' => 20,
			'type' => MENU_LOCAL_TASK,
		);

		$items['linkedin_sso/logout'] = array(
			'title' => 'JSAPI logout',
			'page callback' => '_linkedin_sso_logout_page',
			'type' => MENU_CALLBACK,
			'access callback' => true,
			'weight' => 20
		);
		
		$items['linkedin_sso/login'] = array(
			'title' => 'JSAPI login',
			'page callback' => '_linkedin_sso_login_page',
			'type' => MENU_CALLBACK,
			'access callback' => true,
			'weight' => 20
		);
		
		$items['linkedin_sso/admin'] = array(
			'title' => 'Drupal Login',
			'page arguments' => array('_linkedin_sso_adminlogin_page'),
			'page callback' => 'drupal_get_form',
			'access callback' => true,
			'weight' => 20,
			'type' => MENU_CALLBACK,
		);

	  	return $items;
	}
	
	public function permission() {
		return array(
	    	'administer linkedin_sso' => array(
	      	'title' => t('Administer LinkedIn SSO'),
			),
		);
	}
	
	public function admin_login($form, &$form_state, $action) {
		switch($action) {
			case 'form':
				// generate the normal login form
				$form = user_login($form, $form_state);
				
				// put our validator onto the stack
				array_unshift($form['#validate'], '_linkedin_sso_adminlogin_page_validate');
				
				// return the form for rendering
				return $form;
				break; // good measure ;)
			case 'validate':
				// validate that this login is the root login only
				$user = user_load_by_name($form['name']['#value']);
				if(!$user || $user->uid != 1) {
					form_set_error('username', 'Specified username is not allowed to login from here');
				}
				return;
				break; // good measure ;)
			case 'submit':
				// do the actual login as usual if we made it to this point
				user_login_submit($form, $form_state);
				break;
		}
	}
	
	public function admin_settings($form, &$form_state) {
		$form['linkedin_sso_api_key'] = array(
			'#type' => 'textfield',
			'#title' => t('LinkedIn Developer API Key'),
			'#default_value' => variable_get('linkedin_sso_api_key', ''),
			'#size' => 120,
			'#maxlength' => 1024,
			'#description' => t('Your LinkedIn application API Key. You can create one on https://www.linkedin.com/secure/developer'),
			'#required' => TRUE,
		);

		$form['linkedin_sso_secret_key'] = array(
			'#type' => 'textfield',
			'#title' => t('LinkedIn Developer Secret Key'),
			'#default_value' => variable_get('linkedin_sso_secret_key', ''),
			'#size' => 120,
			'#maxlength' => 1024,
			'#description' => t('Your LinkedIn application Secret Key. You can create one on https://www.linkedin.com/secure/developer'),
			'#required' => TRUE,
		);
		
		$form['linkedin_sso_interstitial_html_head'] = array(
			'#type' => 'textarea',
			'#title' => t('Logout interstitial HTML HEAD'),
			'#default_value' => variable_get('linkedin_sso_interstitial_html_head', ''),
			'#size' => 120,
			'#maxlength' => 16384,
			'#description' => t('Any custom styles or scripts you want to be injected into the HEAD of the logout interstitial page.'),
			'#required' => FALSE,
		);
		
		$form['linkedin_sso_interstitial_html_body'] = array(
			'#type' => 'textarea',
			'#title' => t('Logout interstitial HTML BODY'),
			'#default_value' => variable_get('linkedin_sso_interstitial_html_body', 'Interstitial logout message goes here!'),
			'#size' => 120,
			'#maxlength' => 16384,
			'#description' => t('The HTML for the interstitial page shown when a user logs out.'),
			'#required' => TRUE,
		);

		return system_settings_form($form);
	}
	
	public function login_form_alter(&$form, $form_state, $form_id) {
		if(!$this->api_key || !$this->api_secret) {
			$form['links']['#markup'] = "Please configure the LinkedIn SSO module in the admin section.";
			return;
		}
		
		$form['name']['#type'] = 'hidden';
		$form['name']['#disabled'] = TRUE;

		$form['pass']['#type'] = 'hidden';
		$form['pass']['#disabled'] = TRUE;

		$form['actions']['#type'] = 'hidden';
		$form['actions']['#disabled'] = TRUE;

		$form['links']['#markup'] = '<div id="linkedin_sso"><script type="IN/Login"><a href="' . $GLOBALS["base_url"] . '/linkedin_sso/logout">Logout</a></script></div>';
	}
	
	public function display_name(&$name, $account) {
		if(isset($account->data["linkedin_sso"]["first_name"])) {
			$name = $account->data["linkedin_sso"]["first_name"];
		}
		if(isset($account->data["linkedin_sso"]["last_name"])) {
			$name .= " " . $account->data["linkedin_sso"]["last_name"];
		}
		$name = trim($name);
	}
	
	public function user_logout() {
		if(!$this->api_key || !$this->api_secret) {
			return;
		}
		
		if(isset($_GET["no_jsapi_logout"])) {
			// We aren't doing a JSAPI logout, so no need to do anything special here
			return;
		}
		
		// we need to destroy the session because we are hijacking what the default user_logout does
		session_destroy();

		// rather than redirect to the homepage, redirect here so we can do a JSAPI logout too
		drupal_goto("linkedin_sso/logout");
	}
	
	public function user_logout_html() {
		user_cookie_save(array("linkedin_sso_entropy"=>++$this->linkedin_sso_entropy));
				
		print("<html>\n<head>");
		print(drupal_get_html_head() . "\n");
		print(drupal_get_js() . "\n");
		print(drupal_get_css() . "\n");
		print(variable_get('linkedin_sso_interstitial_html_head', '') . "\n</head>");
		$markup = variable_get('linkedin_sso_interstitial_html_body', '');
		print <<<EOB
<body>
	<script>
		var drupal_linkedin_sso_load_callback = function() {
			IN.User.logout();
		}
	</script>
	{$markup}
</body>
</html>
EOB;
	}
	
	public function user_login_html() {
		$profile = $this->get_user();
		
		if(!$profile) {
			watchdog('linkedin_sso', "LinkedIn SSO failure. Failed to get user profile info!", array(), WATCHDOG_DEBUG);
			drupal_goto("/");
		}
		
		$this->set_member_info_and_login_account($profile);
		
		header("Location: " . $this->validate_return_url($_REQUEST["return"]));
	}
	
	public function menu_local_tasks_alter(&$data, $router_item, $root_path) {
		if(!$this->api_key || !$this->api_secret) {
			return;
		}
		
		switch($root_path) {
			case 'user':
			case 'user/register':
			case 'user/password':
				$data['tabs'] = array();
				break;
		}
	}
	
	public function form_user_profile_form_alter(&$form, $form_state, $form_id) {
		if(!$this->api_key || !$this->api_secret) {
			return;
		}
		
		$form['account']['mail']['#disabled'] = TRUE;
	    $form['account']['mail']['#description'] = t('This email address is automatically set to your primary LinkedIn email address and may not be changed here.');

		$form['account']['current_pass']['#type'] = 'hidden';
		$form['account']['current_pass']['#disabled'] = TRUE;
		$form['account']['pass']['#type'] = 'hidden';
	    $form['account']['pass']['#disabled'] = TRUE;

		$form['picture']['picture_upload']['#type'] = 'hidden';
		$form['picture']['#description'] = t('Your picture is automatically pulled from your LinkedIn profile and may not be changed here.');
	}
	
	private function validate_return_url($url) {
		if(!$url) {
			return $GLOBALS["base_url"];
		}
		
		$host = parse_url($url, PHP_URL_HOST);
		$system_host = parse_url($GLOBALS["base_url"], PHP_URL_HOST);
		if($host === false || $host !== $system_host) {
			return $GLOBALS["base_url"];
		}
		
		return $url;
	}
	
	private function get_user() {
		if(!$this->api_key || !$this->api_secret) {
			watchdog('linkedin_sso', "LinkedIn SSO API key / secret are not configured", array(), WATCHDOG_DEBUG);
			return false;
		}
		
		$cookie_string = isset($_COOKIE["linkedin_oauth_" . $this->api_key]) ? $_COOKIE["linkedin_oauth_" . $this->api_key] : '{}';
		$cookie = json_decode($cookie_string);
		
		if(!$this->validate_oauth_cookie($cookie)) {
			drupal_set_message(t('User account login failed because of system problems.'), 'error');
			return false;
		}
		
		return $this->get_profile_info($cookie->member_id, $cookie->access_token);
	}
	
	private function validate_oauth_cookie($cookie) {
		if(!$this->api_key || !$this->api_secret) {
			watchdog('linkedin_sso', "LinkedIn SSO API key / secret are not configured", array(), WATCHDOG_DEBUG);
			return false;
		}
		
		if(!isset($cookie->signature_version) || $cookie->signature_version != 1) {
			watchdog('linkedin_sso', "LinkedIn SSO failure. signature_version != 1", array(), WATCHDOG_DEBUG);
			return false;
		}
		
		if(!isset($cookie->signature_method) || $cookie->signature_method != "HMAC-SHA1") {
			watchdog('linkedin_sso', "LinkedIn SSO failure. signature_method != HMAC-SHA1", array(), WATCHDOG_DEBUG);
			return false;
		}
		
		$string = "";
		foreach($cookie->signature_order as $field_name) {
			if(!isset($cookie->$field_name)) {
				$watchdog_tokens = array('%fn' => $field_name);
				watchdog('linkedin_sso', "LinkedIn SSO failure. Field specified in signature_order (%fn) doesn't exist in cookie.", $watchdog_tokens, WATCHDOG_DEBUG);
				return false;
			}
			$string .= $cookie->$field_name;
		}
		
		$hash = base64_encode(hash_hmac('sha1', $string, $this->api_secret, true));
		
		if($hash !== $cookie->signature) {
			$watchdog_tokens = array('%h1' => $cookie->signature, '%h2' => $hash);
			watchdog('linkedin_sso', "LinkedIn SSO failure. Cookie hash (%h1) doesn't equal computed hash (%h2)", $watchdog_tokens, WATCHDOG_DEBUG);
			return false;
		}
		
		return true;
	}
	
	private function check_account_exists($member_id) {
		$account = user_load_by_name($member_id);
		if(is_object($account)) {
			if($account->uid == 1) {
				watchdog('linkedin_sso', "Attempt to log in via LinkedIN SSO to user 1. Rejecting!", array(), WATCHDOG_DEBUG);
				return false;
			} else {
				return true;
			}
		} else {
			return false;
		}
	}
	
	private function set_member_info_and_login_account($profile) {
		user_external_login_register($profile->id, 'linkedin_sso');
		
		$edit = array("data" => array("linkedin_sso" => array()));
		$edit["data"]["linkedin_sso"]["profile_url"] = $profile->publicProfileUrl;
		$edit["data"]["linkedin_sso"]["picture_url"] = $profile->pictureUrl;
		$edit["data"]["linkedin_sso"]["member_id"] = $profile->id;
		$edit["data"]["linkedin_sso"]["first_name"] = $profile->firstName;
		$edit["data"]["linkedin_sso"]["last_name"] = $profile->lastName;
		$edit["data"]["linkedin_sso"]["email"] = isset($profile->emailAddress) ? $profile->emailAddress : 'invalid@example.com'; // this field requires special perms
		$edit["data"]["linkedin_sso"]["summary"] = $profile->summary;
		$edit["data"]["linkedin_sso"]["title"] = isset($profile->positions->values[0]->title) ? $profile->positions->values[0]->title : '';
		$edit["data"]["linkedin_sso"]["company"] = isset($profile->positions->values[0]->company->name) ? $profile->positions->values[0]->company->name : '';
		$edit["data"]["linkedin_sso"]["oauth_token"] = $profile->oauth_token;
		$edit["data"]["linkedin_sso"]["oauth_token_secret"] = $profile->oauth_token_secret;
		
		$edit["mail"] = isset($profile->emailAddress) ? $profile->emailAddress : 'invalid@example.com'; // this field requires special perms
		$edit["picture"] = array("uri" => $profile->pictureUrl);

		$ret = user_save($GLOBALS["user"], $edit);
		
		if($ret) {
			$GLOBALS["user"] = $ret;
		} else {
			watchdog('linkedin_sso', "Failed to save extended profile fields for user.", array(), WATCHDOG_DEBUG);
		}
	}
	
	private function get_profile_info($member_id, $access_token) {
		if(!$this->api_key || !$this->api_secret) {
			watchdog('linkedin_sso', "LinkedIn SSO API key / secret are not configured", array(), WATCHDOG_DEBUG);
			return false;
		}
		
		// first do the token upgrade call
		$oauth = new OAuth($this->api_key, $this->api_secret);
		$oauth->fetch($this->access_token_url, array('xoauth_oauth2_access_token' => $access_token), OAUTH_HTTP_METHOD_POST);
		
		// parse out the oauth tokens and tell the OAuth lib about them
		parse_str($oauth->getLastResponse(), $response);
		$oauth->setToken($response['oauth_token'], $response['oauth_token_secret']);
		
		// fetch the user's profile
		$oauth->fetch($this->profile_api_url, array(), OAUTH_HTTP_METHOD_GET, array('x-li-format' => 'json'));
		$profile = json_decode($oauth->getLastResponse());
		
		// add a couple things extra we want to persist
		$profile->oauth_token = $response['oauth_token'];
		$profile->oauth_token_secret = $response['oauth_token_secret'];
		
		return $profile;
	}
}