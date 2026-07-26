require "rake"
require "digest"
require "fileutils"
require "tmpdir"

GITHUB_PAGES_BRANCH = "gh-pages"
PLATFORM = "--platform linux/amd64"
JEKYLL_IMAGE = "jekyll/jekyll:latest"
REPO_DIR = File.expand_path(__dir__)
BUILD_DIR = "#{REPO_DIR}_site"
PAGES_DIR = "#{REPO_DIR}_#{GITHUB_PAGES_BRANCH}"

def sync_changed_files(source_dir, target_dir)
  source_paths = Dir.glob("#{source_dir}/**/*", File::FNM_DOTMATCH)
    .reject { |path| [ ".", ".." ].include? File.basename(path) }
    .reject { |path| File.directory?(path) }
    .map { |path| path.delete_prefix("#{source_dir}/") }

  target_paths = Dir.glob("#{target_dir}/**/*", File::FNM_DOTMATCH)
    .reject { |path| [ ".", ".." ].include? File.basename(path) }
    .reject { |path| File.directory?(path) }
    .map { |path| path.delete_prefix("#{target_dir}/") }
    .reject { |path| path == ".git" || path.start_with?(".git/") }

  (target_paths - source_paths).each do |relative_path|
    FileUtils.rm_f File.join(target_dir, relative_path)
  end

  source_paths.each do |relative_path|
    source_path = File.join(source_dir, relative_path)
    target_path = File.join(target_dir, relative_path)

    next if File.file?(target_path) && Digest::SHA256.file(source_path).hexdigest == Digest::SHA256.file(target_path).hexdigest

    FileUtils.mkdir_p File.dirname(target_path)
    FileUtils.cp source_path, target_path
  end
end

desc "Build the site with Jekyll"
task :build, [ :baseurl ] do |task, args|
  baseurl = args[:baseurl] || ""
  if ENV["LP_USE_DOCKER_INSTEAD_OF_LOCAL_RUBY"] == "true"
    # TODO: does not work on macOS
    sh <<~HERE_DOC
      docker run --rm \
        #{PLATFORM} \
        --volume "#{REPO_DIR}:/srv/jekyll" \
        --volume "#{BUILD_DIR}:/srv/jekyll/_site" \
        -w /srv/jekyll \
        #{JEKYLL_IMAGE} \
        jekyll build \
        --baseurl '#{baseurl}'
    HERE_DOC
  else
    sh "bundle install"
    sh "bundle exec jekyll build -d #{BUILD_DIR} --baseurl '#{baseurl}'"
  end
end

desc "Commit source code to main, rebase, and push"
task :commit_and_push_with_rebase do
  sh "git add ."
  sh %(git commit -m "Update source site content" || echo 'Nothing to commit on main')
  sh "git pull --rebase || echo 'cannot rebase, probably no main branch on remote yet'"
  sh "git push origin main"
end

desc "Deploy to #{GITHUB_PAGES_BRANCH} branch using a cached checkout (does not touch #{BUILD_DIR})"
task :deploy do
  origin = `git config --get remote.origin.url`.strip
  fail "origin is empty" if origin.empty?

  Dir.mktmpdir do |build_dir|
    if ENV["LP_USE_DOCKER_INSTEAD_OF_LOCAL_RUBY"] == "true"
      sh <<~HERE_DOC
        docker run --rm \
          #{PLATFORM} \
          --volume "#{REPO_DIR}:/srv/jekyll" \
          --volume "#{build_dir}:/srv/jekyll/_site" \
          -w /srv/jekyll \
          #{JEKYLL_IMAGE} \
          jekyll build
      HERE_DOC
    else
      sh "bundle exec jekyll build -d #{build_dir}"
    end

    pages_checkout_created = false
    unless File.directory?(File.join(PAGES_DIR, ".git"))
      if system("git clone --branch #{GITHUB_PAGES_BRANCH} --single-branch #{origin} #{PAGES_DIR}")
        puts "Cloned #{GITHUB_PAGES_BRANCH} to #{PAGES_DIR}"
        pages_checkout_created = true
      else
        FileUtils.mkdir_p PAGES_DIR
        Dir.chdir PAGES_DIR do
          sh "git init"
          sh "git checkout --orphan #{GITHUB_PAGES_BRANCH}"
          sh "git remote add origin #{origin}"
        end
        pages_checkout_created = true
      end
    end

    Dir.chdir PAGES_DIR do
      sh "git remote set-url origin #{origin}"
      current_branch = `git branch --show-current`.strip
      sh "git checkout #{GITHUB_PAGES_BRANCH}" unless current_branch == GITHUB_PAGES_BRANCH
      sh "git pull --ff-only origin #{GITHUB_PAGES_BRANCH}" unless pages_checkout_created

      sync_changed_files build_dir, PAGES_DIR

      sh "git add -A"
      if system("git diff --cached --quiet")
        puts "No changes to publish on #{GITHUB_PAGES_BRANCH}"
        next
      end

      sh "git commit -m 'Site updated at #{Time.now.utc}'"

      puts "Pushing to #{origin}"
      sh "git push origin #{GITHUB_PAGES_BRANCH}"
    end
  end
end

desc "Full deploy: commit source and publish site"
task commit_and_push: [ :commit_and_push_with_rebase, :deploy ]

desc "Pull the repo"
task :pull do |task, args|
  sh "git pull --rebase"
end
